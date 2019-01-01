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

/* dialog stuff */
function onLoad()
{
    var args = window.arguments[0];

    window.onAcceptCallback = args.onOk;
    window.calendarEvent = args.calendarEvent;
    window.originalRecurrenceInfo = args.recurrenceInfo;

    window.removedExceptions = [];
    window.addedExceptions = [];

    loadDialog();

    updateDeck();

    updateDuration();

    updateAccept();

    opener.setCursor("auto");

    checkSelectedException();

    self.focus();
}

function onAccept()
{
    var event = window.calendarEvent;

    var recurrenceInfo = saveDialog();

    window.onAcceptCallback(recurrenceInfo);

    return true;
}

function onCancel()
{

}

function loadDialog()
{
    // Start with setting some labels, that depend on the (start)date of the item
    // Those labels are for the monthly recurrence deck.
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://calendar/locale/dateFormat.properties");

    // Set label to '15th day of the month'
    var nthstr = props.GetStringFromName("ordinal.suffix."+window.calendarEvent.startDate.day);
    var str = props.formatStringFromName("recurNthDay", [window.calendarEvent.startDate.day, nthstr], 2);
    document.getElementById("monthly-nth-day").label = str;

    // Set label to 'second week of the month'
    var monthWeekNum = Math.floor(window.calendarEvent.startDate.day / 7) + 1;
    nthstr = props.GetStringFromName("ordinal.name."+monthWeekNum);
    var daystr = props.GetStringFromName("day."+(window.calendarEvent.startDate.weekday+1)+".name");
    str = props.formatStringFromName("recurNthWeek", [nthstr, daystr], 2);
    document.getElementById("monthly-nth-week").label = str;

    // Set two values needed to create the real rrule later
    document.getElementById("monthly-nth-week").day = window.calendarEvent.startDate.weekday;
    document.getElementById("monthly-nth-week").week = monthWeekNum;

    // If this is the last friday of the month, set label to 'last friday of the month'
    // (Or any other day, ofcourse.) Otherwise, hide last option
    var monthLength = window.calendarEvent.startDate.endOfMonth.day;
    var isLastWeek = (monthLength - window.calendarEvent.startDate.day) < 7;
    document.getElementById("monthly-last-week").hidden = !isLastWeek;
    if (isLastWeek) {
        str = props.formatStringFromName("recurLast", [daystr], 1);
        document.getElementById("monthly-last-week").label = str;
    }

    document.getElementById("monthly-last-week").day = window.calendarEvent.startDate.weekday;


    /* Set a starting value for the exceptions picker */
    var item = window.calendarEvent;
    var date = item.startDate || item.entryDate || item.dueDate;
    document.getElementById("exdate-picker").value = date.jsDate;

    if (!window.originalRecurrenceInfo)
        return;

    /* split out rules and exceptions */
    var rrules = splitRecurrenceRules(window.originalRecurrenceInfo);
    var rules = rrules[0];
    var exceptions = rrules[1];

    /* deal with the rules */
    if (rules.length > 0) {
        // we only handle 1 rule currently
        var rule = rules[0];
        if (rule instanceof calIRecurrenceRule) {

            switch(rule.type) {
            case "DAILY":
                document.getElementById("period-list").selectedIndex = 0;

                setElementValue("daily-days", rule.interval);
                break;
            case "WEEKLY":
                document.getElementById("period-list").selectedIndex = 1;

                const byDayTable = { 1 : "sun", 2 : "mon", 3 : "tue", 4 : "wed",
                                     5 : "thu", 6 : "fri", 7: "sat" };

                for each (var i in rule.getComponent("BYDAY", {})) {
                    setElementValue("weekly-" + byDayTable[i], "true", "checked");
                }
                break;
            case "MONTHLY":
                document.getElementById("period-list").selectedIndex = 2;
                // XXX This code ignores a lot of monthly recurrence rules that
                // can come in from external sources. There just is no UI to
                // show them
                var days = rule.getComponent("BYMONTHDAY", {});
                if (days.length > 0 && days[0]) {
                    radioGroupSelectItem("monthly-type", "monthly-nth-day");
                }
                days = rule.getComponent("BYDAY", {}) ;
                if (days.length > 0 && days[0] > 0) {
                    radioGroupSelectItem("monthly-type", "monthly-nth-week");
                }
                if (days.length > 0 && days[0] < 0) {
                    radioGroupSelectItem("monthly-type", "monthly-last-week");
                }
                break;
            case "YEARLY":
                document.getElementById("period-list").selectedIndex = 3;
                break;
            default:
                dump("unable to handle your rule type!\n");
                break;
            }

            /* load up the duration of the event radiogroup */
            if (rule.isByCount) {
                if (rule.count == -1) {
                    setElementValue("recurrence-duration", "forever");
                } else {
                    setElementValue("recurrence-duration", "ntimes");
                    setElementValue("repeat-ntimes-count", rule.count );
                }
            } else {
                var endDate = rule.endDate;
                if (!endDate) {
                    setElementValue("recurrence-duration", "forever");
                } else {
                    // convert the datetime from UTC to localtime.
                    endDate = endDate.getInTimezone(calendarDefaultTimezone());
                    setElementValue("recurrence-duration", "until");
                    setElementValue("repeat-until-date", endDate.jsDate);
                }
            }        
        }
    }

    /* Deal with exceptions */
    var exceptionListBox = document.getElementById("recurrence-exceptions-listbox");

    for each (exception in exceptions) {
        if (exception instanceof calIRecurrenceDate) {
            exceptionListBox.appendItem(exception.date.toString()).date = exception.date;
        } else if (exception instanceof calIRecurrenceDateSet) {
            var dateSet = exception.getDates({});
            for each(date in dateSet)
                exceptionListBox.appendItem(date.toString()).date = date;
        } else
            dump(exception);
    }
}

function saveDialog()
{
    // This works, but if we ever support more complex recurrence,
    // e.g. recurrence for Martians, then we're going to want to
    // not clone and just recreate the recurrenceInfo each time.
    // The reason is that the order of items (rules/dates/datesets)
    // matters, so we can't always just append at the end.  This
    // code here always inserts a rule first, because all our
    // exceptions should come afterward.
    var deckNumber = Number(getElementValue("period-list"));
    
    var recurrenceInfo = null;
    if (window.originalRecurrenceInfo) {
        recurrenceInfo = window.originalRecurrenceInfo.clone();
        var rrules = splitRecurrenceRules(recurrenceInfo);
        if (rrules[0].length > 0)
            recurrenceInfo.deleteRecurrenceItem(rrules[0][0]);
    } else {
        recurrenceInfo = createRecurrenceInfo();
        recurrenceInfo.item = window.calendarEvent;
    }

    var recRule = new calRecurrenceRule();
    switch (deckNumber) {
    case 0:
        recRule.type = "DAILY";
        var ndays = Number(getElementValue("daily-days"));
        recRule.interval = ndays;
        break;
    case 1:
        recRule.type = "WEEKLY";
        recRule.interval = 1; // XXX we need to support every 2 weeks and so on..
        var onDays = [];
        ["sun", "mon", "tue", "wed", "thu", "fri", "sat"].
            forEach(function(d)
                    {
                        var elem = document.getElementById("weekly-" + d); 
                        if (elem.checked) {
                            onDays.push(elem.getAttribute("value"));
                        }
                    });
        if (onDays.length > 0)
            recRule.setComponent("BYDAY", onDays.length, onDays);
        break;
    case 2:
        recRule.type = "MONTHLY";
        recRule.interval = 1; // XXX we need to support every 2 months and so on..
        var recurtype = getElementValue("monthly-type");
        switch (recurtype) {
          case "nth-day":
            recRule.setComponent("BYMONTHDAY", 1, [window.calendarEvent.startDate.day]);
            break;
          case "nth-week":
            var el = document.getElementById('monthly-nth-week');
            // For more info on where this magic formula comes from, see icalrecur.c,
            // icalrecurrencetype_day_day_of_week()
            recRule.setComponent("BYDAY", 1, [el.week*8 + el.day+1]);
            break;
          case "last-week":
            el = document.getElementById('monthly-last-week');
            recRule.setComponent("BYDAY", 1, [(-1)*(8+Number(el.day)+1)]);
            break;
        }
        break;
    case 3:
        recRule.type = "YEARLY";
        var nyears = Number(getElementValue("yearly-years"));
        if (nyears == "")
            nyears = 1;
        recRule.interval = nyears;
        break;
    }

    /* figure out how long this event is supposed to last */
    switch(document.getElementById("recurrence-duration").selectedItem.value) {
    case "forever":
        recRule.count = -1;
        break;
    case "ntimes":
        recRule.count = Math.max(1, getElementValue("repeat-ntimes-count"));
        break;
    case "until":
        // get the datetime from the control (which is in localtime),
        // set the time to 23:59:99 and convert that to UTC time.
        var endDate = getElementValue("repeat-until-date")
        endDate.setHours(23);
        endDate.setMinutes(59);
        endDate.setSeconds(59);
        endDate.setMilliseconds(999);
        endDate = jsDateToDateTime(endDate);
        endDate.normalize();
        recRule.endDate = endDate;
        break;
    }

    recurrenceInfo.insertRecurrenceItemAt(recRule, 0);

    for each (date in window.removedExceptions) {
        recurrenceInfo.restoreOccurrenceAt(date);
    }

    var exceptionsBox = document.getElementById("recurrence-exceptions-listbox");
    for each (var ex in window.addedExceptions) {
        var dateitem = new calRecurrenceDate();
        dateitem.isNegative = true;
        dateitem.date = ex;
        recurrenceInfo.appendRecurrenceItem(dateitem);
    }

    return recurrenceInfo;
}


function updateDeck()
{
    document.getElementById("period-deck").selectedIndex = Number(getElementValue("period-list"));

    updateAccept();
}

function updateDuration()
{
    var durationSelection = document.getElementById("recurrence-duration").selectedItem.value;
    if (durationSelection == "forever") {
    }

    if (durationSelection == "ntimes") {
        setElementValue("repeat-ntimes-count", false, "disabled");
    } else {
        setElementValue("repeat-ntimes-count", "true", "disabled");
    }

    if (durationSelection == "until") {
        setElementValue("repeat-until-date", false, "disabled");
    } else {
        setElementValue("repeat-until-date", "true", "disabled");
    }
}

function updateAccept()
{
    var acceptButton = document.getElementById("calendar-recurrence-dialog").getButton("accept");
    acceptButton.removeAttribute("disabled", "true");
    document.getElementById("repeat-interval-warning").setAttribute("hidden", true);
    document.getElementById("repeat-numberoftimes-warning").setAttribute("hidden", true);

    switch (Number(getElementValue("period-list"))) {
    case 0: // daily
        var ndays = Number(getElementValue("daily-days"));
        if (ndays == "" || ndays < 1) {
            document.getElementById("repeat-interval-warning").removeAttribute("hidden");
            acceptButton.setAttribute("disabled", "true");
        }
        break;
    case 3: // yearly
        var nyears = Number(getElementValue("yearly-years"));
        if (nyears == "" || nyears < 1) {
            document.getElementById("repeat-interval-warning").removeAttribute("hidden");
            acceptButton.setAttribute("disabled", "true");
        }
        break;
    }

    if (document.getElementById("recurrence-duration").selectedItem.value == "ntimes") {
        var ntimes = getElementValue("repeat-ntimes-count");
        if (ntimes == "" || ntimes < 1) {
            document.getElementById("repeat-numberoftimes-warning").removeAttribute("hidden");
            acceptButton.setAttribute("disabled", "true");
        }
    }

    this.sizeToContent();
}

function addException() {
    var jsDate = document.getElementById("exdate-picker").value;
    var exDate = jsDateToDateTime(jsDate);
    exDate.isDate = true;
    if (window.calendarEvent.startDate) {
        exDate.timezone = window.calendarEvent.startDate.timezone;
    } else if (window.calendarEvent.entryDate) {
        exDate.timezone = window.calendarEvent.entryDate.timezone;
    } else {
        exDate.timezone = window.calendarEvent.dueDate.timezone;
    }

    window.addedExceptions.push(exDate);

    var exBox = document.getElementById("recurrence-exceptions-listbox");
    var exItem = exBox.appendItem(exDate.toString());
    exItem.date = exDate;

    this.sizeToContent();
}

function removeSelectedException()
{
    var exceptionList = document.getElementById("recurrence-exceptions-listbox");
    var item = exceptionList.selectedItem;

    var addedRecently = false;
    for (var ii in window.addedExceptions) {
        if (window.addedExceptions[ii].compare(item.date) == 0) {
            window.addedExceptions.splice(ii, 1);
            addedRecently = true;
            break;
        }
    }

    if (!addedRecently) {
        window.removedExceptions.push(item.date);
    }
    exceptionList.removeItemAt(exceptionList.getIndexOfItem(item));
    checkSelectedException();
}

function splitRecurrenceRules(recurrenceInfo)
{
    var ritems = recurrenceInfo.getRecurrenceItems({});

    var rules = [];
    var exceptions = [];

    for each (var r in ritems) {
        if (r.isNegative)
            exceptions.push(r);
        else
            rules.push(r);
    }

    return [rules, exceptions];
}
function checkSelectedException()
{
    var exceptionList = document.getElementById("recurrence-exceptions-listbox");
    var item = exceptionList.selectedItem;
    if (!item) {
        document.getElementById("remove-exceptions-button").setAttribute("disabled", "true");
    } else {
        document.getElementById("remove-exceptions-button").removeAttribute("disabled");
    }
}
