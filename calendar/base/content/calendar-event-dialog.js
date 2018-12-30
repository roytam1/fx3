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

var gReadOnlyMode = false;

// If the user clicks 'More', then we set this to true.  Otherwise, we don't
// load/check the stuff in this area, because it hasn't changed.
var gDetailsShown = false;

var gItemDuration;

/* dialog stuff */
function onLoad()
{
    var args = window.arguments[0];

    window.onAcceptCallback = args.onOk;
    window.calendarItem = args.calendarEvent;
    window.mode = args.mode;
    window.recurrenceInfo = null;

    if (window.calendarItem.calendar && window.calendarItem.calendar.readOnly) {
        gReadOnlyMode = true;
    }

    /* add calendars to the calendar menulist */
    var calendarList = document.getElementById("item-calendar");
    var calendars = getCalendarManager().getCalendars({});
    for (i in calendars) {
        var calendar = calendars[i];
        var menuitem = calendarList.appendItem(calendar.name, i);
        menuitem.calendar = calendar;
    }

    loadDialog(window.calendarItem);

    // figure out what the title of the dialog should be and set it
    updateTitle();

    // hide rows based on if this is an event or todo
    updateStyle();

    // update the accept button
    updateAccept();

    // disabling recurring todo until bug 328197 is fixed
    if (isToDo(window.calendarItem))
        disableElement("item-recurrence");

    // update datetime pickers
    updateDueDate();
    updateEntryDate();

    // update datetime pickers
    updateAllDay();

    // update recurrence button
    updateRecurrence();

    // update our size!
    window.sizeToContent();

    document.getElementById("item-title").focus();

    opener.setCursor("auto");

    self.focus();
}

function onAccept()
{
    // if this event isn't mutable, we need to clone it like a sheep
    var originalItem = window.calendarItem;
    var item = (originalItem.isMutable) ? originalItem : originalItem.clone();

    saveDialog(item);

    var calendar = document.getElementById("item-calendar").selectedItem.calendar;

    window.onAcceptCallback(item, calendar, originalItem);

    return true;
}

function onCancel()
{

}

function loadDialog(item)
{
    var kDefaultTimezone = calendarDefaultTimezone();

    setElementValue("item-title",       item.title);
    setElementValue("item-location",    item.getProperty("LOCATION"));

    /* event specific properties */
    if (isEvent(item)) {
        var startDate = item.startDate.getInTimezone(kDefaultTimezone);
        var endDate = item.endDate.getInTimezone(kDefaultTimezone);
        gItemDuration = endDate.subtractDate(startDate);
        
        // Check if an all-day event has been passed in (to adapt endDate).
        if (startDate.isDate) {
            setElementValue("event-all-day", true, "checked");
            endDate.day -= 1;

            // The date/timepicker uses jsDate internally. Because jsDate does
            // not know the concept of dates we end up displaying times unequal
            // to 00:00 for all-day events depending on local timezone setting. 
            // Calling normalize() recalculates that times to represent 00:00
            // in local timezone.
            endDate.normalize();
            startDate.normalize();
        }
        
        setElementValue("event-starttime",   startDate.jsDate);
        setElementValue("event-endtime",     endDate.jsDate);
        document.getElementById("component-type").selectedIndex = 0;
    }

    /* todo specific properties */
    if (isToDo(item)) {
        var hasEntryDate = (item.entryDate != null);
        setElementValue("todo-has-entrydate", hasEntryDate, "checked");
        if (hasEntryDate)
            setElementValue("todo-entrydate", item.entryDate.jsDate);

        var hasDueDate = (item.dueDate != null);
        setElementValue("todo-has-duedate", hasDueDate, "checked");
        if (hasDueDate)
            setElementValue("todo-duedate", item.dueDate.jsDate);
        if (hasEntryDate && hasDueDate) {
            gItemDuration = item.dueDate.subtractDate(item.entryDate);
        }
        document.getElementById("component-type").selectedIndex = 1;
    }

    /* item default calendar */
    // If this is a new item, it might not have a calendar, but a default
    // option could well have been passed in.
    var calendarToUse = item.calendar || window.arguments[0].calendar
    if (calendarToUse) {
        var calendarList = document.getElementById("item-calendar");
        var calendars = getCalendarManager().getCalendars({});
        for (i in calendars) {
            if (calendarToUse.uri.equals(calendars[i].uri))
                calendarList.selectedIndex = i;
        }
    } else {
        // no calendar attached to item
        // select first entry in calendar list as default
        document.getElementById("item-calendar").selectedIndex = 0;
    }

    /* Categories */
    try {
        var pb2 = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch2);
        var categoriesString = pb2.getComplexValue("calendar.categories.names", 
                                                  Components.interfaces.nsISupportsString).data;
        var categoriesList = categoriesString.split( "," );

        // insert the category already in the menulist so it doesn't get lost
        var itemCategory = item.getProperty("CATEGORIES");
        if (itemCategory) {
            if (categoriesString.indexOf(itemCategory) == -1)
                categoriesList[categoriesList.length] = itemCategory;
        }
        categoriesList.sort();

        var oldMenulist = document.getElementById("item-categories");
        while (oldMenulist.hasChildNodes()) {
            oldMenulist.removeChild(oldMenulist.lastChild);
        }

        var categoryMenuList = document.getElementById("item-categories");
        var indexToSelect = 0;

        // Add a 'none' option to allow users to cancel the category
        var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                            .getService(Components.interfaces.nsIStringBundleService);
        var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
        var noneItem = categoryMenuList.appendItem(props.GetStringFromName("None"), "NONE");

        for (var i in categoriesList) {
            var catItem = categoryMenuList.appendItem(categoriesList[i], categoriesList[i]);
            catItem.value = categoriesList[i];
            if (itemCategory && categoriesList[i] == itemCategory) {
                indexToSelect = parseInt(i)+1;  // Add 1 because of 'None'
            }
        }
        categoryMenuList.selectedIndex = indexToSelect;

    } catch (ex) {
        // The app using this dialog doesn't support categories
        document.getElementById("categories-box").collapsed = true;
    }


    /* recurrence */
    /* if the item is a proxy occurrence/instance, a few things aren't valid:
     * - Setting recurrence on the item
     * - changing the calendar
     */
    if (item.parentItem != item) {
        setElementValue("item-recurrence", "true", "disabled");
        setElementValue("set-recurrence", "true", "disabled");
        setElementValue("item-calendar", "true", "disabled");
    } else if (item.recurrenceInfo)
        setElementValue("item-recurrence", "true", "checked");

    var detailsButton = document.getElementById("calendar-event-dialog").getButton("disclosure");
    var detailsElements = document.getElementsByAttribute("details", "true");

    if (document.getElementById("description-row").getAttribute("collapsed") != "true") {
        detailsButton.setAttribute("label", lessLabel);
        for each (elem in detailsElements) {
            elem.collapsed = false;
        }
        loadDetails();
    } else {
        for each (elem in detailsElements) {
            elem.collapsed = true;
        }
        detailsButton.setAttribute("label", moreLabel);
    }
}

function saveDialog(item)
{
    setItemProperty(item, "title",       getElementValue("item-title"));
    setItemProperty(item, "LOCATION",    getElementValue("item-location"));

    var kDefaultTimezone = calendarDefaultTimezone();

    if (isEvent(item)) {
        var startDate = jsDateToDateTime(getElementValue("event-starttime"));
        var endDate = jsDateToDateTime(getElementValue("event-endtime"));
        startDate = startDate.getInTimezone(kDefaultTimezone);
        endDate = endDate.getInTimezone(kDefaultTimezone);

        var isAllDay = getElementValue("event-all-day", "checked");
        if (isAllDay) {
            startDate.isDate = true;
            startDate.normalize();

            endDate.isDate = true;
            endDate.day += 1;
            endDate.normalize();
        }

        setItemProperty(item, "startDate",   startDate);
        setItemProperty(item, "endDate",     endDate);
    }

    if (isToDo(item)) {
        var entryDate = getElementValue("todo-has-entrydate", "checked") ? 
            jsDateToDateTime(getElementValue("todo-entrydate")) : null;
        if (entryDate) {
            entryDate = entryDate.getInTimezone(kDefaultTimezone);
        }
        setItemProperty(item, "entryDate",   entryDate);

        var dueDate = getElementValue("todo-has-duedate", "checked") ? 
            jsDateToDateTime(getElementValue("todo-duedate")) : null;
        if (dueDate) {
            dueDate = dueDate.getInTimezone(kDefaultTimezone);
        }
        setItemProperty(item, "dueDate",     dueDate);

        var percentCompleteInteger = 0;
        if (getElementValue("percent-complete-textbox") != "") {
            percentCompleteInteger = parseInt(getElementValue("percent-complete-textbox"));
        }
        if (percentCompleteInteger < 0) {
            percentCompleteInteger = 0;
        } else if (percentCompleteInteger > 100) {
            percentCompleteInteger = 100;
        }
        setItemProperty(item, "PERCENT-COMPLETE", percentCompleteInteger);
    }

    /* recurrence */
    if (getElementValue("item-recurrence", "checked")) {
        if (window.recurrenceInfo) {
            item.recurrenceInfo = window.recurrenceInfo;
        }
    } else {
        item.recurrenceInfo = null;
    }

    /* Category */
    var category = getElementValue("item-categories");

    if (category != "NONE") {
       setItemProperty(item, "CATEGORIES", category);
    } else {
       item.deleteProperty("CATEGORIES");
    }

    if (!gDetailsShown) {
        // We never showed the items in the 'More' box.  That means that clone()
        // took care of it, so just return now
        dump(item.icalString + '\n');
        return;
    }

    setItemProperty(item, "URL",         getElementValue("item-url"));
    setItemProperty(item, "DESCRIPTION", getElementValue("item-description"));

    var status;
    if (isEvent(item)) {
        status = getElementValue("event-status");
    } else {
        status = getElementValue("todo-status");
    }

    setItemProperty(item, "STATUS",   status);
    setItemProperty(item, "PRIORITY", getElementValue("priority-levels"));
    setItemProperty(item, "CLASS", getElementValue("privacy-menulist"));

    if (item.status == "COMPLETED" && isToDo(item)) {
        item.completedDate = jsDateToDateTime(getElementValue("completed-date-picker"));
    }

    /* attendence */
    item.removeAllAttendees();
    var attendeeListBox = document.getElementById("attendees-listbox");
    for each (kid in attendeeListBox.childNodes) {
        if (kid.attendee) {
            item.addAttendee(kid.attendee);
        }
    }

    /* alarms */
    var hasAlarm = (getElementValue("item-alarm") != "none");
    if (!hasAlarm) {
        item.alarmOffset = null;
        item.alarmLastAck = null;
        item.alarmRelated = null;
    } else {
        var alarmLength = getElementValue("alarm-length-field");
        var alarmUnits = document.getElementById("alarm-length-units").selectedItem.value;
        if (document.getElementById("alarm-trigger-relation").selectedItem.value == "START") {
            item.alarmRelated = item.ALARM_RELATED_START;
        } else {
            item.alarmRelated = item.ALARM_RELATED_END;
        }
        var duration = Components.classes["@mozilla.org/calendar/duration;1"]
                                 .createInstance(Components.interfaces.calIDuration);
        if (item.alarmRelated == item.ALARM_RELATED_START) {
            duration.isNegative = true;
        }
        duration[alarmUnits] = alarmLength;
        duration.normalize();

        item.alarmOffset = duration;
    }

    dump(item.icalString + "\n");
}

function updateTitle()
{
    var isNew = window.calendarItem.isMutable;
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/calendar.properties");
    if (isEvent(window.calendarItem)) {
        if (isNew)
            document.title = props.GetStringFromName("newEventDialog");
        else
            document.title = props.GetStringFromName("editEventDialog");
    } else if (isToDo(window.calendarItem)) {
        if (isNew)
            document.title = props.GetStringFromName("newTaskDialog");
        else
            document.title = props.GetStringFromName("editTaskDialog");
    }
}

function updateComponentType() {
    //XXX We still can't properly convert from event <-> task via QI
    return;
}

function updateStyle()
{
    const kDialogStylesheet = "chrome://calendar/content/calendar-event-dialog.css";

    for each(var stylesheet in document.styleSheets) {
        if (stylesheet.href == kDialogStylesheet) {
            if (isEvent(window.calendarItem))
                stylesheet.insertRule(".todo-only { display: none; }", 0);
            else if (isToDo(window.calendarItem))
                stylesheet.insertRule(".event-only { display: none; }", 0);
            return;
        }
    }
}

function onStartTimeChange()
{
    if (!gItemDuration) {
        return;
    }
    var startWidgetId;
    var endWidgetId;
    if (isEvent(window.calendarItem)) {
        startWidgetId = "event-starttime";
        endWidgetId = "event-endtime";
    } else {
        if (!getElementValue("todo-has-entrydate", "checked") || !getElementValue("todo-has-duedate", "checked")) {
            gItemDuration = null;
            return;
        }
        startWidgetId = "todo-entrydate";
        endWidgetId = "todo-duedate";
    }
    var start = jsDateToDateTime(getElementValue(startWidgetId));
    start.addDuration(gItemDuration);
    setElementValue(endWidgetId, start.getInTimezone(calendarDefaultTimezone()).jsDate);
    updateAccept();
}

function onEndTimeChange()
{
    var startWidgetId;
    var endWidgetId;
    if (isEvent(window.calendarItem)) {
        startWidgetId = "event-starttime";
        endWidgetId = "event-endtime";
    } else {
        if (!getElementValue("todo-has-entrydate", "checked") || 
            !getElementValue("todo-has-duedate", "checked")) {
            gItemDuration = null;
            return;
        }
        startWidgetId = "todo-entrydate";
        endWidgetId = "todo-duedate";
    }
    var start = jsDateToDateTime(getElementValue(startWidgetId));
    var end = jsDateToDateTime(getElementValue(endWidgetId));
    gItemDuration = end.subtractDate(start);
    updateAccept();
}
function updateAccept()
{
    var kDefaultTimezone = calendarDefaultTimezone();
    var acceptButton = document.getElementById("calendar-event-dialog").getButton("accept");

    var title = getElementValue("item-title");
    if (title.length == 0)
        acceptButton.setAttribute("disabled", "true");
    else if (acceptButton.getAttribute("disabled"))
        acceptButton.removeAttribute("disabled");

    // don't allow for end dates to be before start dates
    var startDate;
    var endDate;
    if (isEvent(window.calendarItem)) {
        startDate = jsDateToDateTime(getElementValue("event-starttime"));
        endDate = jsDateToDateTime(getElementValue("event-endtime"));

        // For all-day events we are not interested in times and compare only dates.
        if (getElementValue("event-all-day", "checked")) {
            // jsDateToDateTime returnes the values in UTC. Depending on the local
            // timezone and the values selected in datetimepicker the date in UTC
            // might be shifted to the previous or next day.
            // For example: The user (with local timezone GMT+05) selected 
            // Feb 10 2006 00:00:00. The corresponding value in UTC is
            // Feb 09 2006 19:00:00. If we now set isDate to true we end up with
            // a date of Feb 09 2006 instead of Feb 10 2006 resulting in errors
            // during the following comparison.
            // Calling getInTimezone() ensures that we use the same dates as 
            // displayed to the user in datetimepicker for comparison.
            startDate = startDate.getInTimezone(kDefaultTimezone);
            endDate = endDate.getInTimezone(kDefaultTimezone);
            startDate.isDate = true;
            endDate.isDate = true;
        }
    } else {
        startDate = getElementValue("todo-has-entrydate", "checked") ? 
            jsDateToDateTime(getElementValue("todo-entrydate")) : null;
        endDate = getElementValue("todo-has-duedate", "checked") ? 
            jsDateToDateTime(getElementValue("todo-duedate")) : null;
    }

    var timeWarning = document.getElementById("end-time-warning");
    if (endDate && startDate && endDate.compare(startDate) == -1) {
        acceptButton.setAttribute("disabled", "true");
        timeWarning.removeAttribute("hidden");
    } else {
        acceptButton.removeAttribute("disabled");
        timeWarning.setAttribute("hidden", "true");
    }

    // can't add/edit items in readOnly calendars
    document.getElementById("read-only-item").setAttribute("hidden", !gReadOnlyMode);
    var cal = document.getElementById("item-calendar").selectedItem.calendar;
    document.getElementById("read-only-cal").setAttribute("hidden", 
                                              !cal.readOnly);
    if (gReadOnlyMode || cal.readOnly) {
        acceptButton.setAttribute("disabled", "true");
    }

    if (!updateTaskAlarmWarnings()) {
        acceptButton.setAttribute("disabled", "true");
    }
    return;
}

function updateDueDate()
{
    if (!isToDo(window.calendarItem))
        return;

    // force something to get set if there was nothing there before
    setElementValue("todo-duedate", getElementValue("todo-duedate"));

    setElementValue("todo-duedate", !getElementValue("todo-has-duedate", "checked"), "disabled");
    if (getElementValue("todo-has-entrydate", "checked") && getElementValue("todo-has-duedate", "checked")) {
        var start = jsDateToDateTime(getElementValue("todo-entrydate"));
        var end = jsDateToDateTime(getElementValue("todo-duedate"));
        gItemDuration = end.subtractDate(start);
    } else {
        gItemDuration = null;
    }

    updateAccept();
}

function updateEntryDate()
{
    if (!isToDo(window.calendarItem))
        return;

    // force something to get set if there was nothing there before
    setElementValue("todo-entrydate", getElementValue("todo-entrydate"));

    setElementValue("todo-entrydate", !getElementValue("todo-has-entrydate", "checked"), "disabled");

    if (getElementValue("todo-has-entrydate", "checked") && getElementValue("todo-has-duedate", "checked")) {
        var start = jsDateToDateTime(getElementValue("todo-entrydate"));
        var end = jsDateToDateTime(getElementValue("todo-duedate"));
        gItemDuration = end.subtractDate(start);
    } else {
        gItemDuration = null;
    }

    updateAccept();
}

function updateTaskAlarmWarnings() {
    document.getElementById("alarm-start-warning").setAttribute("hidden", true);
    document.getElementById("alarm-end-warning").setAttribute("hidden", true);

    var alarmType = getElementValue("item-alarm");
    if (!gDetailsShown || !isToDo(window.calendarItem) || alarmType == "none") {
        return true;
    }

    var hasEntryDate = getElementValue("todo-has-entrydate", "checked");
    var hasDueDate = getElementValue("todo-has-duedate", "checked");

    var alarmRelated = document.getElementById("alarm-trigger-relation").selectedItem.value;

    if ((alarmType != "custom" || alarmRelated == "START") && !hasEntryDate) {
        document.getElementById("alarm-start-warning").removeAttribute("hidden");
        return false;
    }

    if (alarmRelated == "END" && !hasDueDate) {
        document.getElementById("alarm-end-warning").removeAttribute("hidden");
        return false;
    }

    return true;
}

function updateAllDay()
{
    if (!isEvent(window.calendarItem))
        return;

    var allDay = getElementValue("event-all-day", "checked");
    setElementValue("event-starttime", allDay, "timepickerdisabled");
    setElementValue("event-endtime", allDay, "timepickerdisabled");

    updateAccept();
}


function updateRecurrence()
{
    var recur = getElementValue("item-recurrence", "checked");
    if (recur) {
        setElementValue("set-recurrence", false, "disabled");
    } else {
        setElementValue("set-recurrence", "true", "disabled");
    }
}

var prevAlarmItem = null;
function setAlarmFields(alarmItem)
{
    var alarmLength = alarmItem.getAttribute("length");
    if (alarmLength != "") {
        var alarmUnits = alarmItem.getAttribute("unit");
        var alarmRelation = alarmItem.getAttribute("relation");
        setElementValue("alarm-length-field", alarmLength);
        setElementValue("alarm-length-units", alarmUnits);
        setElementValue("alarm-trigger-relation", alarmRelation);
    }
}
function updateAlarm()
{
    var alarmMenu = document.getElementById("item-alarm");
    var alarmItem = alarmMenu.selectedItem;

    var alarmItemValue = alarmItem.getAttribute("value");
    switch (alarmItemValue) {
    case "custom":
        /* restore old values if they're around */
        setAlarmFields(alarmItem);
        
        document.getElementById("alarm-details").removeAttribute("hidden");
        break;
    default:
        var customItem = document.getElementById("alarm-custom-menuitem");
        if (prevAlarmItem == customItem) {
            customItem.setAttribute("length", getElementValue("alarm-length-field"));
            customItem.setAttribute("unit", getElementValue("alarm-length-units"));
            customItem.setAttribute("relation", getElementValue("alarm-trigger-relation"));
        }
        setAlarmFields(alarmItem);

        document.getElementById("alarm-details").setAttribute("hidden", true);
        break;
    }

    prevAlarmItem = alarmItem;
    updateAccept();

    this.sizeToContent();
}

function editRecurrence()
{
    var args = new Object();
    args.calendarEvent = window.calendarItem;
    args.recurrenceInfo = window.recurrenceInfo || args.calendarEvent.recurrenceInfo;

    var savedWindow = window;
    args.onOk = function(recurrenceInfo) {
        savedWindow.recurrenceInfo = recurrenceInfo;
    };

    // wait cursor will revert to auto in eventDialog.js loadCalendarEventDialog
    window.setCursor("wait");

    // open the dialog modally
    openDialog("chrome://calendar/content/calendar-recurrence-dialog.xul", "_blank", "chrome,titlebar,modal", args);
}



/* utility functions */
function setItemProperty(item, propertyName, value)
{
    switch(propertyName) {
    case "startDate":
        if (value.isDate && !item.startDate.isDate ||
            !value.isDate && item.startDate.isDate ||
            value.timezone != item.startDate.timezone ||
            value.compare(item.startDate) != 0)
            item.startDate = value;
        break;
    case "endDate":
        if (value.isDate && !item.endDate.isDate ||
            !value.isDate && item.endDate.isDate ||
            value.timezone != item.endDate.timezone ||
            value.compare(item.endDate) != 0)
            item.endDate = value;
        break;

    case "entryDate":
        if (value == item.entryDate)
            break;
        if ((value && !item.entryDate) ||
            (!value && item.entryDate) ||
            (value.timezone != item.entryDate.timezone) ||
            (value.compare(item.entryDate) != 0))
            item.entryDate = value;
        break;
    case "dueDate":
        if (value == item.dueDate)
            break;
        if ((value && !item.dueDate) ||
            (!value && item.dueDate) ||
            (value.timezone != item.dueDate.timezone) ||
            (value.compare(item.dueDate) != 0))
            item.dueDate = value;
        break;
    case "isCompleted":
        if (value != item.isCompleted)
            item.isCompleted = value;
        break;

    case "title":
        if (value != item.title)
            item.title = value;
        break;

    default:
        if (!value || value == "")
            item.deleteProperty(propertyName);
        else if (item.getProperty(propertyName) != value)
            item.setProperty(propertyName, value);
        break;
    }
}

function toggleDetails() {
    var detailsElements = document.getElementsByAttribute("details", "true");
    var detailsButton = document.getElementById("calendar-event-dialog").getButton("disclosure");

    if (!detailsElements[0].collapsed) {
        // Hide details
        for each (elem in detailsElements) {
            elem.collapsed = true;
        }
        detailsButton.setAttribute("label", moreLabel);
        this.sizeToContent();
        return;
    }

    // Display details
    for each (elem in detailsElements) {
        elem.collapsed = false;
    }
    detailsButton.setAttribute("label", lessLabel);
    this.sizeToContent();

    if (gDetailsShown) {
        // We've already loaded this stuff before, so we're done
        return;
    }

    loadDetails();
}

function loadDetails() {
    gDetailsShown = true;
    var item = window.calendarItem;

    /* attendence */
    var attendeeString = "";
    var attendeeListBox = document.getElementById("attendees-listbox");

    var child;
    while ((child = attendeeListBox.lastChild) && (child.tagName == "listitem")) {
        attendeeListBox.removeChild(child);
    }

    for each (var attendee in item.getAttendees({})) {
        var listItem = document.createElement("listitem");
        var nameCell = document.createElement("listcell");
        nameCell.setAttribute("label", attendee.id.split("MAILTO:")[1]);
        listItem.appendChild(nameCell);

        listItem.attendee = attendee;
        attendeeListBox.appendChild(listItem);
    }


    /* Status */
    setElementValue("item-url",         item.getProperty("URL"));
    setElementValue("item-description", item.getProperty("DESCRIPTION"));
    if (isEvent(item)) {
        setElementValue("event-status", item.getProperty("STATUS"));
    } else {
        setElementValue("todo-status", item.getProperty("STATUS"));
    }

    /* Task completed date */
    if (item.completedDate) {
        updateToDoStatus(item.status, item.completedDate.jsDate);
    } else {
        updateToDoStatus(item.status);
    }

    /* Task percent complete */
    if (isToDo(item)) {
        var percentCompleteInteger = 0;
        var percentCompleteProperty = item.getProperty("PERCENT-COMPLETE");
        if (percentCompleteProperty != null) {
            percentCompleteInteger = parseInt(percentCompleteProperty);
        }
        if (percentCompleteInteger < 0) {
            percentCompleteInteger = 0;
        } else if (percentCompleteInteger > 100) {
            percentCompleteInteger = 100;
        }
        setElementValue("percent-complete-textbox", percentCompleteInteger);
    }

    /* Priority */
    var priorityInteger = parseInt(item.priority);
    if (priorityInteger >= 1 && priorityInteger <= 4) {
        document.getElementById("priority-levels").selectedIndex = 3; // high priority
    } else if (priorityInteger == 5) {
        document.getElementById("priority-levels").selectedIndex = 2; // medium priority
    } else if (priorityInteger >= 6 && priorityInteger <= 9) {
        document.getElementById("priority-levels").selectedIndex = 1; // low priority
    } else {
        document.getElementById("priority-levels").selectedIndex = 0; // not defined
    }

    /* Privacy */
    switch (item.privacy) {
        case "PUBLIC":
        case "PRIVATE":
        case "CONFIDENTIAL":
            setElementValue("privacy-menulist", item.privacy);
            break;
        case "":
            setElementValue("private-menulist", "PUBLIC");
            break;
        default:  // bogus value
            dump("ERROR! Event has invalid privacy string: " + item.privacy + "\n");
            break;
    }

    /* alarms */
    if (item.alarmOffset) {
        var alarmRelatedStart = (item.alarmRelated == item.ALARM_RELATED_START);
        if (alarmRelatedStart) {
            setElementValue("alarm-trigger-relation", "START");
        } else {
            setElementValue("alarm-trigger-relation", "END");
        }

        var offset = item.alarmOffset;
        if (offset.minutes) {
            var minutes = offset.minutes + offset.hours*60 + offset.days*24*60 + offset.weeks*60*24*7;
            // Special cases for the common alarms
            if ((minutes == 15) && alarmRelatedStart) {
                document.getElementById("item-alarm").selectedIndex = 2;
            } else if ((minutes == 30) && alarmRelatedStart) {
                document.getElementById("item-alarm").selectedIndex = 3;
            } else {
                setElementValue("alarm-length-field", minutes);
                setElementValue("alarm-length-units", "minutes");
                setElementValue("item-alarm", "custom");
            }
        } else if (offset.hours) {
            var hours = offset.hours + offset.days*24 + offset.weeks*24*7;
            setElementValue("alarm-length-field", hours);
            setElementValue("alarm-length-units", "hours");
            setElementValue("item-alarm", "custom");
        } else { // days
            var days = offset.days + offset.weeks*7;
            setElementValue("alarm-length-field", days);
            setElementValue("alarm-length-units", "days");
            setElementValue("item-alarm", "custom");
        }
    }

    // update alarm checkbox/label/settings button
    updateAlarm();

    updateTaskAlarmWarnings();
}

function updateToDoStatus(status, passedInCompletedDate)
{
    // RFC2445 doesn't support completedDates without the todo's status
    // being "COMPLETED", however twiddling the status menulist shouldn't
    // destroy that information at this point (in case you change status
    // back to COMPLETED). When we go to store this VTODO as .ics the
    // date will get lost.

    var completedDate;
    if (passedInCompletedDate) {
        completedDate = passedInCompletedDate;
    } else {
        completedDate = null;
    }

    // remember the original values
    var oldPercentComplete = getElementValue("percent-complete-textbox");
    var oldCompletedDate   = getElementValue("completed-date-picker");

    switch (status) {
    case "":
    case "NONE":
        document.getElementById("todo-status").selectedIndex = 0;
        disableElement("percent-complete-textbox");
        disableElement("percent-complete-label");
        break;
    case "CANCELLED":
        document.getElementById("todo-status").selectedIndex = 4;
        disableElement("percent-complete-textbox");
        disableElement("percent-complete-label");
        break;
    case "COMPLETED":
        document.getElementById("todo-status").selectedIndex = 3;
        enableElement("percent-complete-textbox");
        enableElement("percent-complete-label");
        // if there isn't a completedDate, set it to now
        if (!completedDate)
            completedDate = new Date();
        break;
    case "IN-PROCESS":
        document.getElementById("todo-status").selectedIndex = 2;
        disableElement("completed-date-picker");
        enableElement("percent-complete-textbox");
        enableElement("percent-complete-label");
        break;
    case "NEEDS-ACTION":
        document.getElementById("todo-status").selectedIndex = 1;
        enableElement("percent-complete-textbox");
        enableElement("percent-complete-label");
        break;
    }

    if (status == "COMPLETED") {
        setElementValue("percent-complete-textbox", "100");
        setElementValue("completed-date-picker", completedDate);
        enableElement("completed-date-picker");
    } else {
        setElementValue("percent-complete-textbox", oldPercentComplete);
        setElementValue("completed-date-picker", oldCompletedDate);
        disableElement("completed-date-picker");
    }
}
