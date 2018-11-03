// Agenda tree view, to display upcoming events, tasks, and reminders
//
// We track three periods of time for a segmented view:
// - today: the current time until midnight
// - tomorrow: midnight to midnight
// - soon: end-of-tomorrow to end-of-one-week-from-today (midnight)
//
// Events (recurrences of events, really) are stored in per-period containers,
// hung off of "period" objects. In addition, we build an array of the row-
// representation we use for backing the tree display.
//
// The tree-view array (this.events) consists of the synthetic events for the time
// periods, each one followed, if tree-expanded, by its collection of events.  This
// results in a this.events array like the following, if "Today" and "Soon" are
// expanded:
// [ synthetic("Today"),
//   occurrence("Today Event 1"),
//   occurrence("Today Event 2"),
//   synthetic("Tomorrow"),
//   synthetic("Soon"),
//   occurrence("Soon Event 1"),
//   occurrence("Soon Event 2") ]
//
// At window load, we connect the view to the tree and initiate a calendar query
// to populate the event buckets.  Once the query is complete, we sort each bucket
// and then build the aggregate array described above.
//
// When calendar queries are refreshed (by a calendar being added/removed WRT the
// current view, the user selecting a different filter, or some hidden manual-
// refresh testing UI) the event buckets are emptied, and we add items as they
// arrive.
//

function Synthetic(title, open)
{
    this.title = title;
    this.open = open;
    this.events = [];
}

var agendaTreeView = {
    events: [],
    todayCount: 0,
    tomorrowCount: 0,
    soonCount: 0,
    prevRowCount: 0
};

agendaTreeView.init =
function initAgendaTree()
{
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://lightning/locale/lightning.properties");

    this.today = new Synthetic(props.GetStringFromName("agendaToday"), true);
    this.tomorrow = new Synthetic(props.GetStringFromName("agendaTomorrow"), false);
    this.soon = new Synthetic(props.GetStringFromName("agendaSoon"), false);
    this.periods = [this.today, this.tomorrow, this.soon];
}

agendaTreeView.addEvents =
function addEvents(master)
{
    this.events.push(master);
    if (master.open)
        this.events = this.events.concat(master.events);
};

agendaTreeView.rebuildEventsArray =
function rebuildEventsArray()
{
    this.events = [];
    this.addEvents(this.today);
    this.addEvents(this.tomorrow);
    this.addEvents(this.soon);
};

agendaTreeView.forceTreeRebuild =
function forceTreeRebuild()
{
    if (this.tree) {
        this.tree.view = this;
    }
};

agendaTreeView.rebuildAgendaView =
function rebuildAgendaView(invalidate)
{
    this.rebuildEventsArray();
    this.forceTreeRebuild();
};

agendaTreeView.__defineGetter__("rowCount",
function get_rowCount()
{
    return this.events.length;
});

agendaTreeView.isContainer =
function isContainer(row)
{
    return (this.events[row] instanceof Synthetic);
};

agendaTreeView.isContainerOpen =
function isContainerOpen(row)
{
    var open = this.events[row].open;
    return open;
};

agendaTreeView.isContainerEmpty =
function isContainerEmpty(row)
{
    if (this.events[row].events.length == 0)
        return true;
    return false;
};

agendaTreeView.setTree =
function setTree(tree)
{
    this.tree = tree;
};

agendaTreeView.getCellText =
function getCellText(row, column)
{
    var event = this.events[row];
    if (column.id == "col-agenda-item") {
        if (event instanceof Synthetic)
            return event.title;
        return event.title;
    }

    if (event instanceof Synthetic)
        return "";
    var start = event.startDate || event.dueDate;
    var dateFormatter = Components.classes["@mozilla.org/calendar/datetime-formatter;1"]
                                  .getService(Components.interfaces.calIDateTimeFormatter);
    start = start.getInTimezone(calendarDefaultTimezone());
    return dateFormatter.formatDateTime(start);
};

agendaTreeView.getLevel =
function getLevel(row)
{
    if (this.isContainer(row))
        return 0;
    return 1;
};

agendaTreeView.isSorted =
function isSorted() { return false; };

agendaTreeView.isEditable =
function isEditable(row, column) { return false; };

agendaTreeView.isSeparator =
function isSeparator(row) { return false; };

agendaTreeView.getImageSrc =
function getImageSrc(row, column) { return null; };

agendaTreeView.getCellProperties =
function getCellProperties(row, column) { return null; };

agendaTreeView.getRowProperties =
function getRowProperties(row) { return null; };

agendaTreeView.getColumnProperties =
function getColumnProperties(column) { return null; };

agendaTreeView.cycleHeader =
function cycleHeader(header)
{
    this.refreshCalendarQuery(); // temporary hackishness
    this.rebuildAgendaView();
    this.forceTreeRebuild();
};

agendaTreeView.getParentIndex =
function getParentIndex(row)
{
    if (this.isContainer(row))
        return -1;
    var i = row - 1;
    do {
        if (this.events[i] instanceof Synthetic)
            return i;
        i--;
    } while (i != -1);
    throw "no parent for row " + row + "?";
};

agendaTreeView.toggleOpenState =
function toggleOpenState(row)
{
    if (!this.isContainer(row))
        throw "toggling open state on non-container row " + row + "?";
    var header = this.events[row];
    if (!("open") in header)
        throw "no open state found on container row " + row + "?";
    header.open = !header.open;
    this.rebuildAgendaView(); // reconstruct the visible row set
    this.forceTreeRebuild();
};

agendaTreeView.hasNextSibling =
function hasNextSibling(row, afterIndex)
{
};

agendaTreeView.findPeriodForItem =
function findPeriodForItem(item)
{
    var start = item.startDate || item.dueDate;
    if (!start) 
        return null;
    if (start.compare(this.today.end) == -1)
        return this.today;
        
    if (start.compare(this.tomorrow.end) == -1)
        return this.tomorrow;
    
    if (start.compare(this.soon.end) == -1)
        return this.soon;
    

    return null;
};

agendaTreeView.addItem =
function addItem(item)
{
    var when = this.findPeriodForItem(item);
    if (!when)
        return;
    when.events.push(item);
    this.calendarUpdateComplete();
};

agendaTreeView.onDoubleClick =
function agendaDoubleClick(event)
{
    // We only care about left-clicks
    if (event.button != 0) 
        return;

    // Find the row clicked on, and the corresponding event
    var tree = document.getElementById("agenda-tree");
    var row = tree.treeBoxObject.getRowAt(event.clientX, event.clientY);
    var calendar = ltnSelectedCalendar();
    var calEvent = this.events[row];

    if (!calEvent) { // Clicked in empty space, just create a new event
        createEventWithDialog(calendar, today(), today());
        return;
    }
    if (!this.isContainer(row)) { // Clicked on a task/event, edit it
        var eventToEdit = getOccurrenceOrParent(calEvent);
        modifyEventWithDialog(eventToEdit);
    } else { // Clicked on a container, create an event that day
        if (calEvent == this.today) {
            createEventWithDialog(calendar, today(), today());
        } else {
            var tom = today().clone();
            var offset = (calEvent == this.tomorrow) ? 1 : 2;
            tom.day += offset;
            tom.normalize()
            createEventWithDialog(calendar, tom, tom);
        }
    }
}

agendaTreeView.deleteItem =
function deleteItem(item)
{
    var when = this.findPeriodForItem(item);
    if (!when) {
        return;
    }
    
    when.events = when.events.filter(function (e) {
                                         if (e.id != item.id)
                                             return true;
                                         if (e.recurrenceId && item.recurrenceId &&
                                             e.recurrenceId.compare(item.recurrenceId) != 0)
                                             return true;
                                         return false;
                                     });
    this.rebuildAgendaView(true);
};

agendaTreeView.calendarUpdateComplete =
function calendarUpdateComplete()
{
    [this.today, this.tomorrow, this.soon].forEach(function(when) {
        function compare(a, b) {
            var ad = a.startDate || a.dueDate;
            var bd = b.startDate || b.dueDate;
            return ad.compare(bd);
        }
        when.events.sort(compare);
    });
    this.rebuildAgendaView(true);
};

agendaTreeView.calendarOpListener =
{
    agendaTreeView: agendaTreeView
};

agendaTreeView.calendarOpListener.onOperationComplete =
function listener_onOperationComplete(calendar, status, optype, id,
                                      detail)
{
    this.agendaTreeView.calendarUpdateComplete();  
};

agendaTreeView.calendarOpListener.onGetResult =
function listener_onGetResult(calendar, status, itemtype, detail, count, items)
{
    if (!Components.isSuccessCode(status))
        return;
    
    items.forEach(this.agendaTreeView.addItem, this.agendaTreeView);
};

agendaTreeView.refreshCalendarQuery =
function refreshCalendarQuery()
{
    var filter = this.calendar.ITEM_FILTER_COMPLETED_ALL |
                 this.calendar.ITEM_FILTER_CLASS_OCCURRENCES;
    if (!this.filterType)
        this.filterType = 'all';
    switch (this.filterType) {
        case 'all': 
            filter |= this.calendar.ITEM_FILTER_TYPE_EVENT |
                      this.calendar.ITEM_FILTER_TYPE_TODO;
            break;
        case 'events':
            filter |= this.calendar.ITEM_FILTER_TYPE_EVENT;
            break;
        case 'tasks':
            filter |= this.calendar.ITEM_FILTER_TYPE_TODO;
            break;
    }

    this.periods.forEach(function (p) { p.events = []; });
    this.calendar.getItems(filter, 0, this.today.start, this.soon.end,
                           this.calendarOpListener);
};

agendaTreeView.updateFilter =
function updateAgendaFilter(menulist) {
    this.filterType = menulist.selectedItem.value;
    this.refreshCalendarQuery();
    return;
};

agendaTreeView.refreshPeriodDates =
function refreshPeriodDates()
{
    var now = new Date();
    var d = new CalDateTime();
    d.jsDate = now;
    d = d.getInTimezone(calendarDefaultTimezone());

    // Today: now until midnight of tonight
    this.today.start = d.clone();
    d.hour = d.minute = d.second = 0;
    d.day++;
    d.normalize();
    this.today.end = d.clone();

    // Tomorrow: midnight of next day to +24 hrs
    this.tomorrow.start = d.clone();
    d.day++;
    d.normalize();
    this.tomorrow.end = d.clone();

    // Soon: end of tomorrow to 6 six days later (remainder of the week period)
    this.soon.start = d.clone();
    d.day += 6;
    d.normalize();
    this.soon.end = d.clone();

    this.refreshCalendarQuery();
};

agendaTreeView.calendarObserver = {
    agendaTreeView: agendaTreeView
};

// calIObserver:
agendaTreeView.calendarObserver.onStartBatch = function() {};
agendaTreeView.calendarObserver.onEndBatch = function() {};
agendaTreeView.calendarObserver.onLoad = function() {};

agendaTreeView.calendarObserver.onAddItem =
function observer_onAddItem(item)
{
    var occs = item.getOccurrencesBetween(this.agendaTreeView.today.start,
                                          this.agendaTreeView.soon.end, {});
    occs.forEach(this.agendaTreeView.addItem, this.agendaTreeView);
    this.agendaTreeView.rebuildAgendaView();
};

agendaTreeView.calendarObserver.onDeleteItem =
function observer_onDeleteItem(item, rebuildFlag)
{
    var occs = item.getOccurrencesBetween(this.agendaTreeView.today.start,
                                          this.agendaTreeView.soon.end, {});
    occs.forEach(this.agendaTreeView.deleteItem, this.agendaTreeView);
    if (rebuildFlag != "no-rebuild")
        this.agendaTreeView.rebuildAgendaView();
};

agendaTreeView.calendarObserver.onModifyItem =
function observer_onModifyItem(newItem, oldItem)
{
    this.onDeleteItem(oldItem, "no-rebuild");
    this.onAddItem(newItem);
};

agendaTreeView.calendarObserver.onAlarm = function(item) {};
agendaTreeView.calendarObserver.onError = function(errno, msg) {};

agendaTreeView.setCalendar =
function setCalendar(calendar)
{
    if (this.calendar)
        this.calendar.removeObserver(this.calendarObserver);
    this.calendar = calendar;
    calendar.addObserver(this.calendarObserver);

    this.init();

    // Update everything
    this.refreshPeriodDates();
};

function setAgendaTreeView()
{
    agendaTreeView.setCalendar(getCompositeCalendar());
    document.getElementById("agenda-tree").view = agendaTreeView;
}

window.addEventListener("load", setAgendaTreeView, false);
