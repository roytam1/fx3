/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

const CI = Components.interfaces;

const ITIP_HANDLER_MIMETYPE = "application/x-itip-internal";
const ITIP_HANDLER_PROTOCOL = "moz-cal-handle-itip";

const CAL_ITIP_PROTO_HANDLER_CID =
    Components.ID("{6E957006-B4CE-11D9-B053-001124736B74}");
const CAL_ITIP_PROTO_HANDLER_CONTRACTID =
    "@mozilla.org/network/protocol;1?name=" + ITIP_HANDLER_PROTOCOL;

const CALMGR_CONTRACTID = "@mozilla.org/calendar/manager;1";

const ItipProtocolHandlerFactory =
{
    createInstance: function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        return (new ItipProtocolHandler()).QueryInterface(iid);
    }
};

const CAL_ITIP_CONTENT_HANDLER_CID =
    Components.ID("{47C31F2B-B4DE-11D9-BFE6-001124736B74}");
const CAL_ITIP_CONTENT_HANDLER_CONTRACTID =
    "@mozilla.org/uriloader/content-handler;1?type=" +
    ITIP_HANDLER_MIMETYPE;

const ItipContentHandlerFactory =
{
    createInstance: function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;

        return (new ItipContentHandler()).QueryInterface(iid);
    }
};

function NYI()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

function ItipChannel(URI)
{
   this.URI = this.originalURI = URI;
}

ItipChannel.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsIChannel) &&
            !aIID.equals(CI.nsIRequest))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        
        return this;
    },
    
    contentType: ITIP_HANDLER_MIMETYPE,
    loadAttributes: null,
    contentLength: 0,
    owner: null,
    loadGroup: null,
    notificationCallbacks: null,
    securityInfo: null,
    
    open: NYI,
    asyncOpen: function (observer, ctxt) {
        observer.onStartRequest(this, ctxt);
    },
    asyncRead: function (listener, ctxt) {
        return listener.onStartRequest(this, ctxt);
    },
    
    isPending: function () { return true; },
    status: Components.results.NS_OK,
    cancel: function (status) { this.status = status; },
    suspend: NYI,
    resume: NYI,
};

function ItipProtocolHandler() { }

ItipProtocolHandler.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsIProtocolHandler))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        return this;
    },
    
    protocolFlags: CI.nsIProtocolHandler.URI_NORELATIVE,
    allowPort: function () { return false; },
    isSecure: false,
    newURI: function (spec, charSet, baseURI)
    {
        var cls = Components.classes["@mozilla.org/network/standard-url;1"];
        var url = cls.createInstance(CI.nsIStandardURL);
        url.init(CI.nsIStandardURL.URLTYPE_STANDARD, 0, spec, charSet, baseURI);
        dump("Creating new URI for " + spec + "\n");
        return url.QueryInterface(CI.nsIURI);
    },
    
    newChannel: function (URI) {
        dump("Creating new ItipChannel for " + URI + "\n");
        return new ItipChannel(URI);
    },
};

function ItipContentHandler() { }

ItipContentHandler.prototype = {
    QueryInterface: function (aIID) {
        if (!aIID.equals(CI.nsISupports) &&
            !aIID.equals(CI.nsIContentHandler))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        
        return this;
    },

    handleContent: function (contentType, windowTarget, request)
    {
        dump("Handling some itip content, whee\n");
        var channel = request.QueryInterface(CI.nsIChannel);
        var uri = channel.URI.spec;
        if (uri.indexOf(ITIP_HANDLER_PROTOCOL + ":") != 0) {
            dump("unexpected uri " + uri + "\n");
            return Components.results.NS_ERROR_FAILURE;
        }
        // moz-cal-handle-itip:///?
        var paramString = uri.substring(ITIP_HANDLER_PROTOCOL.length + 4);
        var paramArray = paramString.split("&");
        var paramBlock = { };
        paramArray.forEach(function (v) {
            var parts = v.split("=");
            paramBlock[parts[0]] = unescape(unescape(parts[1]));
            });
        // dump("content-handler: have params " + paramBlock.toSource() + "\n");
        var event = Components.classes["@mozilla.org/calendar/event;1"].
            createInstance(CI.calIEvent);
        event.icalString = paramBlock.data;
        dump("Processing iTIP event '" + event.title + "' from " +
            event.organizer.id + " (" + event.id + ")\n");
        var calMgr = Components.classes[CALMGR_CONTRACTID].getService(CI.calICalendarManager);
        var cals = calMgr.getCalendars({});
        cals[0].addItem(event, null);
    }
};

var myModule = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        debug("*** Registering Lightning " + ITIP_HANDLER_PROTOCOL + ": handler\n");
        compMgr = compMgr.QueryInterface(CI.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(CAL_ITIP_PROTO_HANDLER_CID,
                                        "Lightning " + ITIP_HANDLER_PROTOCOL + ": handler",
                                        CAL_ITIP_PROTO_HANDLER_CONTRACTID,
                                        fileSpec, location, type);
        debug("*** Registering Lightning " + ITIP_HANDLER_MIMETYPE + " handler\n");
        compMgr.registerFactoryLocation(CAL_ITIP_CONTENT_HANDLER_CID,
                                        "Lightning " + ITIP_HANDLER_MIMETYPE + " handler",
                                        CAL_ITIP_CONTENT_HANDLER_CONTRACTID,
                                        fileSpec, location, type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (cid.equals(CAL_ITIP_PROTO_HANDLER_CID))
            return ItipProtocolHandlerFactory;

        if (cid.equals(CAL_ITIP_CONTENT_HANDLER_CID))
            return ItipContentHandlerFactory;

       throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return myModule;
}
