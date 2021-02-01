/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
 *   Joey Minta <jminta@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const kHoursBetweenUpdates = 6;

function newTimerWithCallback(callback, delay, repeating)
{
    var timer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
    
    timer.initWithCallback(callback,
                           delay,
                           (repeating) ? timer.TYPE_REPEATING_PRECISE : timer.TYPE_ONE_SHOT);
    return timer;
}

function jsDateToDateTime(date)
{
    var newDate = Components.classes["@mozilla.org/calendar/datetime;1"].createInstance(Components.interfaces.calIDateTime);
    newDate.jsDate = date;
    return newDate;
}

function jsDateToFloatingDateTime(date)
{
    var newDate = Components.classes["@mozilla.org/calendar/datetime;1"].createInstance(Components.interfaces.calIDateTime);
    newDate.timezone = "floating";
    newDate.year = date.getFullYear();
    newDate.month = date.getMonth();
    newDate.day = date.getDate();
    newDate.hour = date.getHours();
    newDate.minute = date.getMinutes();
    newDate.second = date.getSeconds();
    newDate.normalize();
    return newDate;
}

function calAlarmService() {
    this.wrappedJSObject = this;

    this.calendarObserver = {
        alarmService: this,

        // calIObserver:
        onStartBatch: function() { },
        onEndBatch: function() { },
        onLoad: function() { },
        onAddItem: function(aItem) {
            if (aItem.alarmOffset)
                this.alarmService.addAlarm(aItem);
        },
        onModifyItem: function(aNewItem, aOldItem) {
            this.alarmService.removeAlarm(aOldItem);

            if (aNewItem.alarmOffset)
                this.alarmService.addAlarm(aNewItem);
        },
        onDeleteItem: function(aDeletedItem) {
            this.alarmService.removeAlarm(aDeletedItem);
        },
        onError: function(aErrNo, aMessage) { }
    };


    this.calendarManagerObserver = {
        alarmService: this,

        onCalendarRegistered: function(aCalendar) {
            this.alarmService.observeCalendar(aCalendar);
        },
        onCalendarUnregistering: function(aCalendar) {
            this.alarmService.unobserveCalendar(aCalendar);
        },
        onCalendarDeleting: function(aCalendar) {},
        onCalendarPrefSet: function(aCalendar, aName, aValue) {},
        onCalendarPrefDeleting: function(aCalendar, aName) {}
    };
}

var calAlarmServiceClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIAlarmService,
            Components.interfaces.nsIObserver,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/alarm-service;1",
    classDescription: "Calendar Alarm Service",
    classID: Components.ID("{7a9200dd-6a64-4fff-a798-c5802186e2cc}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calAlarmService.prototype = {
    mRangeEnd: null,
    mEvents: {},
    mObservers: [],
    mUpdateTimer: null,
    mStarted: false,

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calAlarmServiceClassInfo;

        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calIAlarmService) &&
            !aIID.equals(Components.interfaces.nsIObserver))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },


    /* nsIObserver */
    observe: function (subject, topic, data) {
        if (topic == "profile-after-change") {
            this.shutdown();
            this.startup();
        }
        if (topic == "xpcom-shutdown") {
            this.shutdown();
        }
    },

    /* calIAlarmService APIs */
    mTimezone: null,
    get timezone() {
        return this.mTimezone;
    },

    set timezone(aTimezone) {
        this.mTimezone = aTimezone;
    },

    snoozeEvent: function(event, duration) {
        /* modify the event for a new alarm time */
        var newEvent = event.clone();
        var alarmTime = jsDateToDateTime((new Date())).getInTimezone("UTC");

        // Set the last acknowledged time to now.
        newEvent.alarmLastAck = alarmTime;
        alarmTime.addDuration(duration);

        var datetime;
        if (newEvent.alarmRelated == Components.interfaces.calIItemBase.ALARM_RELATED_START) {
            datetime = newEvent.startDate || newEvent.entryDate;
        } else {
            datetime = newEvent.endDate || newEvent.dueDate;
        }

        var offset = datetime.subtractDate(alarmTime);
        newEvent.alarmOffset = offset;
        // calling modifyItem will cause us to get the right callback
        // and update the alarm properly
        newEvent.calendar.modifyItem(newEvent, event, null);
    },

    addObserver: function(aObserver) {
        dump("observer added\n");
        if (this.mObservers.indexOf(aObserver) != -1)
            return;

        this.mObservers.push(aObserver);
    },

    removeObserver: function(aObserver) {
        dump("observer removed\n");
        function notThis(v) {
            return v != aObserver;
        }

        this.mObservers = this.mObservers.filter(notThis);
    },


    /* helper functions */
    notifyObservers: function(functionName, args) {
        function notify(obs) {
            try { obs[functionName].apply(obs, args);  }
            catch (e) { }
        }
        this.mObservers.forEach(notify);
    },

    startup: function() {
        if (this.mStarted)
            return;

        if (!this.mTimezone) {
            throw Components.results.NS_ERROR_NOT_INITIALIZED;
        }

        dump("Starting calendar alarm service\n");

        var observerSvc = Components.classes["@mozilla.org/observer-service;1"]
                          .getService
                          (Components.interfaces.nsIObserverService);

        observerSvc.addObserver(this, "profile-after-change", false);
        observerSvc.addObserver(this, "xpcom-shutdown", false);

        /* Tell people that we're alive so they can start monitoring alarms.
         * Make sure to do this before calling findAlarms().
         */
        this.notifier = Components.classes["@mozilla.org/embedcomp/appstartup-notifier;1"].getService(Components.interfaces.nsIObserver);
        var notifier = this.notifier;
        notifier.observe(null, "alarm-service-startup", null);

        this.calendarManager = Components.classes["@mozilla.org/calendar/manager;1"].getService(Components.interfaces.calICalendarManager);
        var calendarManager = this.calendarManager;
        calendarManager.addObserver(this.calendarManagerObserver);

        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            this.observeCalendar(calendar);
        }

        this.findAlarms();

        /* set up a timer to update alarms every N hours */
        var timerCallback = {
            alarmService: this,
            notify: function(timer) {
                this.alarmService.findAlarms();
            }
        };

        this.mUpdateTimer = newTimerWithCallback(timerCallback, kHoursBetweenUpdates * 3600000, true);

        this.mStarted = true;
    },

    shutdown: function() {
        /* tell people that we're no longer running */
        var notifier = this.notifier;
        notifier.observe(null, "alarm-service-shutdown", null);

        if (this.mUpdateTimer) {
            this.mUpdateTimer.cancel();
            this.mUpdateTimer = null;
        }
        
        var calendarManager = this.calendarManager;
        calendarManager.removeObserver(this.calendarManagerObserver);

        for each(var timer in this.mEvents) {
            timer.cancel();
        }
        this.mEvents = {};

        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            this.unobserveCalendar(calendar);
        }

        this.calendarManager = null;
        this.notifier = null;
        this.mRangeEnd = null;

        var observerSvc = Components.classes["@mozilla.org/observer-service;1"]
                          .getService
                          (Components.interfaces.nsIObserverService);

        observerSvc.removeObserver(this, "profile-after-change");
        observerSvc.removeObserver(this, "xpcom-shutdown");

        this.mStarted = false;
    },


    observeCalendar: function(calendar) {
        calendar.addObserver(this.calendarObserver);
    },

    unobserveCalendar: function(calendar) {
        calendar.removeObserver(this.calendarObserver);
    },

    addAlarm: function(aItem) {
        var alarmTime;
        if (aItem.alarmRelated == Components.interfaces.calIItemBase.ALARM_RELATED_START) {
            alarmTime = aItem.startDate || aItem.entryDate;
        } else {
            alarmTime = aItem.endDate || aItem.dueDate;
        }

        alarmTime = alarmTime.clone();

        // Handle all day events.  This is kinda weird, because they don't have
        // a well defined startTime.  We just consider the start/end to be 
        // midnight in the user's timezone.
        if (alarmTime.isDate) {
            alarmTime = alarmTime.getInTimezone(this.mTimezone);
            alarmTime.isDate = false;
        }

        alarmTime.addDuration(aItem.alarmOffset);
        alarmTime = alarmTime.getInTimezone("UTC");
dump("considering alarm for item:"+aItem.title+'\n offset:'+aItem.alarmOffset+', which makes alarm time:'+alarmTime+'\n');
        var now;
        // XXX When the item is floating, should use the default timezone
        // from the prefs, instead of the javascript timezone (which is what
        // jsDateToFloatingDateTime uses)
        if (alarmTime.timezone == "floating")
            now = jsDateToFloatingDateTime((new Date()));
        else
            now = jsDateToDateTime((new Date())).getInTimezone("UTC");
dump("now is "+now+'\n');
        var callbackObj = {
            alarmService: this,
            item: aItem,
            notify: function(timer) {
                this.alarmService.alarmFired(this.item);
                delete this.alarmService.mEvents[this.item];
            }
        };

        if (alarmTime.compare(now) >= 0) {
dump("alarm is in the future\n");
            // We assume that future alarms haven't been acknowledged

            // delay is in msec, so don't forget to multiply
            var timeout = alarmTime.subtractDate(now).inSeconds * 1000;

            var timeUntilRefresh = this.mRangeEnd.subtractDate(now).inSeconds * 1000;
            if (timeUntilRefresh < timeout) {
dump("alarm is too late\n");
                // we'll get this alarm later.  No sense in keeping an extra timeout
                return;
            }

            this.mEvents[aItem.id] = newTimerWithCallback(callbackObj, timeout, false);
            dump("adding alarm timeout (" + timeout + ") for " + aItem + "\n");
        } else {
            // This alarm is in the past.  See if it has been previously ack'd
            if (aItem.alarmLastAck && aItem.alarmLastAck.compare(alarmTime) >= 0) {
dump(aItem.title+' - alarm previously ackd\n');
                return;
            } else { // Fire!
dump("alarm is in the past, and unack'd, firing now!\n");
                this.alarmFired(aItem);
            }
        }
    },

    removeAlarm: function(aItem) {
        if (aItem.id in this.mEvents) {
            this.mEvents[aItem.id].cancel();
            delete this.mEvents[aItem.id];
        }
    },

    findAlarms: function() {
        var getListener = {
            alarmService: this,
            onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
            },
            onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
                for (var i = 0; i < aCount; ++i) {
                    var item = aItems[i];
                    if (item.alarmOffset) {
                        this.alarmService.addAlarm(item);
                    }
                }
            }
        };

        var now = jsDateToDateTime((new Date())).getInTimezone("UTC");

        var start;
        if (!this.mRangeEnd) {
            // This is our first search for alarms.  We're going to look for
            // alarms +/- 1 month from now.  If someone sets an alarm more than
            // a month ahead of an event, or doesn't start Sunbird/Lightning
            // for a month, they'll miss some, but that's a slim chance
            start = now.clone();
            start.month -= 1;
            start.normalize();
        } else {
            // This is a subsequent search, so we got all the past alarms before
            start = this.mRangeEnd.clone();
        }
        var until = now.clone();
        until.month += 1;
        until.normalize();

        // We don't set timers for every future alarm, only those within 6 hours
        var end = now.clone();
        end.hour += kHoursBetweenUpdates;
        end.normalize();
        this.mRangeEnd = end.getInTimezone("UTC");

        var calendarManager = this.calendarManager;
        var calendars = calendarManager.getCalendars({});
        for each(var calendar in calendars) {
            calendar.getItems(calendar.ITEM_FILTER_TYPE_ALL | calendar.ITEM_FILTER_CLASS_OCCURRENCES,
                              0, start, until, getListener);
        }
    },

    alarmFired: function(event) {
        if (event.calendar.suppressAlarms)
            return;

        this.notifyObservers("onAlarm", [event]);
    }
};
