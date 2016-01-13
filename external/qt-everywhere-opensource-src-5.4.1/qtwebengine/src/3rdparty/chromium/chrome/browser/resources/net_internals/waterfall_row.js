// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WaterfallRow = (function() {
  'use strict';

  /**
   * A WaterfallRow represents the row corresponding to a single SourceEntry
   * displayed by the EventsWaterfallView.  All times are relative to the
   * "base time" (Time of first event from any source).
   *
   * @constructor
   */

  // TODO(viona):
  // -Support nested events.
  // -Handle updating length when an event is stalled.
  function WaterfallRow(parentView, sourceEntry) {
    this.parentView_ = parentView;
    this.sourceEntry_ = sourceEntry;

    this.description_ = sourceEntry.getDescription();

    this.createRow_();
  }

  // Offset of popup from mouse location.
  var POPUP_OFFSET_FROM_CURSOR = 25;

  WaterfallRow.prototype = {
    onSourceUpdated: function() {
      this.updateRow();
    },

    updateRow: function() {
      var scale = this.parentView_.getScaleFactor();
      // In some cases, the REQUEST_ALIVE event has been received, while the
      // URL Request to start the job has not been received. In that case, the
      // description obtained is incorrect. The following fixes that.
      if (this.description_ == '') {
        this.sourceCell_.innerHTML = '';
        this.description_ = this.sourceEntry_.getDescription();
        addTextNode(this.sourceCell_, this.description_);
      }

      this.rowCell_.innerHTML = '';

      var matchingEventPairs =
          WaterfallRow.findUrlRequestEvents(this.sourceEntry_);

      // Creates the spacing in the beginning to show start time.
      var sourceStartTicks = this.getStartTicks();
      var frontNode = addNode(this.rowCell_, 'div');
      frontNode.classList.add('waterfall-view-padding');
      setNodeWidth(frontNode, sourceStartTicks * scale);

      var barCell = addNode(this.rowCell_, 'div');
      barCell.classList.add('waterfall-view-bar');

      if (this.sourceEntry_.isError()) {
        barCell.classList.add('error');
      }

      var currentEnd = sourceStartTicks;

      for (var i = 0; i < matchingEventPairs.length; ++i) {
        var event = matchingEventPairs[i];
        var startTicks =
            timeutil.convertTimeTicksToRelativeTime(event.startEntry.time);
        var endTicks =
            timeutil.convertTimeTicksToRelativeTime(event.endEntry.time);
        event.eventType = event.startEntry.type;
        event.startTime = startTicks;
        event.endTime = endTicks;
        event.eventDuration = event.endTime - event.startTime;

        // Handles the spaces between events.
        if (currentEnd < event.startTime) {
          var eventDuration = event.startTime - currentEnd;
          var padNode = this.createNode_(
              barCell, eventDuration, this.sourceTypeString_, 'source');
        }

        // Creates event bars.
        var eventNode = this.createNode_(
            barCell, event.eventDuration, EventTypeNames[event.eventType],
            event);
        currentEnd = event.startTime + event.eventDuration;
      }

      // Creates a bar for the part after the last event.
      var endTime = this.getEndTicks();
      if (endTime > currentEnd) {
        var endDuration = endTime - currentEnd;
        var endNode = this.createNode_(
            barCell, endDuration, this.sourceTypeString_, 'source');
      }
    },

    getStartTicks: function() {
      return timeutil.convertTimeTicksToRelativeTime(
                 this.sourceEntry_.getStartTicks());
    },

    getEndTicks: function() {
      return timeutil.convertTimeTicksToRelativeTime(
                 this.sourceEntry_.getEndTicks());
    },

    clearPopup_: function(parentNode) {
      parentNode.innerHTML = '';
    },

    createPopup_: function(parentNode, event, eventType, duration, mouse) {
      var newPopup = addNode(parentNode, 'div');
      newPopup.classList.add('waterfall-view-popup');

      var popupList = addNode(newPopup, 'ul');
      popupList.classList.add('waterfall-view-popup-list');

      popupList.style.maxWidth =
          $(WaterfallView.MAIN_BOX_ID).offsetWidth * 0.5 + 'px';

      this.createPopupItem_(
          popupList, 'Has Error', this.sourceEntry_.isError());

      this.createPopupItem_(popupList, 'Event Type', eventType);

      if (event != 'source') {
        this.createPopupItem_(
            popupList, 'Event Duration', duration.toFixed(0) + 'ms');
        this.createPopupItem_(
            popupList, 'Event Start Time', event.startTime + 'ms');
        this.createPopupItem_(
            popupList, 'Event End Time', event.endTime + 'ms');
      }
      this.createPopupItem_(popupList, 'Source Duration',
                            this.getEndTicks() - this.getStartTicks() + 'ms');
      this.createPopupItem_(popupList, 'Source Start Time',
                            this.getStartTicks() + 'ms');
      this.createPopupItem_(popupList, 'Source End Time',
                            this.getEndTicks() + 'ms');
      this.createPopupItem_(
          popupList, 'Source ID', this.sourceEntry_.getSourceId());
      var urlListItem = this.createPopupItem_(
          popupList, 'Source Description', this.description_);

      urlListItem.classList.add('waterfall-view-popup-list-url-item');

      // Fixes cases where the popup appears 'off-screen'.
      var popupLeft = mouse.pageX - newPopup.offsetWidth;
      if (popupLeft < 0) {
        popupLeft = mouse.pageX;
      }
      newPopup.style.left = popupLeft +
          $(WaterfallView.MAIN_BOX_ID).scrollLeft -
          $(WaterfallView.BAR_TABLE_ID).offsetLeft + 'px';

      var popupTop = mouse.pageY - newPopup.offsetHeight -
          POPUP_OFFSET_FROM_CURSOR;
      if (popupTop < 0) {
        popupTop = mouse.pageY;
      }
      newPopup.style.top = popupTop +
          $(WaterfallView.MAIN_BOX_ID).scrollTop -
          $(WaterfallView.BAR_TABLE_ID).offsetTop + 'px';
    },

    createPopupItem_: function(parentPopup, key, popupInformation) {
      var popupItem = addNode(parentPopup, 'li');
      addTextNode(popupItem, key + ': ' + popupInformation);
      return popupItem;
    },

    createRow_: function() {
      // Create a row.
      var tr = addNode($(WaterfallView.BAR_TBODY_ID), 'tr');
      tr.classList.add('waterfall-view-table-row');

      // Creates the color bar.

      var rowCell = addNode(tr, 'td');
      rowCell.classList.add('waterfall-view-row');
      this.rowCell_ = rowCell;

      this.sourceTypeString_ = this.sourceEntry_.getSourceTypeString();

      var infoTr = addNode($(WaterfallView.INFO_TBODY_ID), 'tr');
      infoTr.classList.add('waterfall-view-information-row');

      var idCell = addNode(infoTr, 'td');
      idCell.classList.add('waterfall-view-id-cell');
      var idValue = this.sourceEntry_.getSourceId();
      var idLink = addNodeWithText(idCell, 'a', idValue);
      idLink.href = '#events&s=' + idValue;

      var sourceCell = addNode(infoTr, 'td');
      sourceCell.classList.add('waterfall-view-url-cell');
      addTextNode(sourceCell, this.description_);
      this.sourceCell_ = sourceCell;

      this.updateRow();
    },

    // Generates nodes.
    createNode_: function(parentNode, duration, eventTypeString, event) {
      var linkNode = addNode(parentNode, 'a');
      linkNode.href = '#events&s=' + this.sourceEntry_.getSourceId();

      var scale = this.parentView_.getScaleFactor();
      var newNode = addNode(linkNode, 'div');
      setNodeWidth(newNode, duration * scale);
      newNode.classList.add(eventTypeToCssClass_(eventTypeString));
      newNode.classList.add('waterfall-view-bar-component');
      newNode.addEventListener(
          'mouseover',
          this.createPopup_.bind(this, newNode, event, eventTypeString,
              duration),
          true);
      newNode.addEventListener(
          'mouseout', this.clearPopup_.bind(this, newNode), true);
      return newNode;
    },
  };

  /**
   * Identifies source dependencies and extracts events of interest for use in
   * inlining in URL Request bars.
   * Created as static function for testing purposes.
   */
  WaterfallRow.findUrlRequestEvents = function(sourceEntry) {
    var eventPairs = [];
    if (!sourceEntry) {
      return eventPairs;
    }

    // One level down from URL Requests.

    var httpStreamJobSources = findDependenciesOfType_(
        sourceEntry, EventType.HTTP_STREAM_REQUEST_BOUND_TO_JOB);

    var httpTransactionReadHeadersPairs = findEntryPairsFromSourceEntries_(
        [sourceEntry], EventType.HTTP_TRANSACTION_READ_HEADERS);
    eventPairs = eventPairs.concat(httpTransactionReadHeadersPairs);

    var proxyServicePairs = findEntryPairsFromSourceEntries_(
        httpStreamJobSources, EventType.PROXY_SERVICE);
    eventPairs = eventPairs.concat(proxyServicePairs);

    if (httpStreamJobSources.length > 0) {
      for (var i = 0; i < httpStreamJobSources.length; ++i) {
        // Two levels down from URL Requests.

        var hostResolverImplSources = findDependenciesOfType_(
            httpStreamJobSources[i], EventType.HOST_RESOLVER_IMPL);

        var socketSources = findDependenciesOfType_(
            httpStreamJobSources[i], EventType.SOCKET_POOL_BOUND_TO_SOCKET);

        // Three levels down from URL Requests.

        // TODO(mmenke):  Some of these may be nested in the PROXY_SERVICE
        //                event, resulting in incorrect display, since nested
        //                events aren't handled.
        var hostResolverImplRequestPairs = findEntryPairsFromSourceEntries_(
            hostResolverImplSources, EventType.HOST_RESOLVER_IMPL_REQUEST);
        eventPairs = eventPairs.concat(hostResolverImplRequestPairs);

        // Truncate times of connection events such that they don't occur before
        // the HTTP_STREAM_JOB event or the PROXY_SERVICE event.
        // TODO(mmenke):  The last HOST_RESOLVER_IMPL_REQUEST may still be a
        //                problem.
        var minTime = httpStreamJobSources[i].getLogEntries()[0].time;
        if (proxyServicePairs.length > 0)
          minTime = proxyServicePairs[0].endEntry.time;
        // Convert to number so comparisons will be numeric, not string,
        // comparisons.
        minTime = Number(minTime);

        var tcpConnectPairs = findEntryPairsFromSourceEntries_(
            socketSources, EventType.TCP_CONNECT);

        var sslConnectPairs = findEntryPairsFromSourceEntries_(
            socketSources, EventType.SSL_CONNECT);

        var connectionPairs = tcpConnectPairs.concat(sslConnectPairs);

        // Truncates times of connection events such that they are shown after a
        // proxy service event.
        for (var k = 0; k < connectionPairs.length; ++k) {
          var eventPair = connectionPairs[k];
          var eventInRange = false;
          if (eventPair.startEntry.time >= minTime) {
            eventInRange = true;
          } else if (eventPair.endEntry.time > minTime) {
            eventInRange = true;
            // Should not modify original object.
            eventPair.startEntry = shallowCloneObject(eventPair.startEntry);
            // Need to have a string, for consistency.
            eventPair.startEntry.time = minTime + '';
          }
          if (eventInRange) {
            eventPairs.push(eventPair);
          }
        }
      }
    }
    eventPairs.sort(function(a, b) {
      return a.startEntry.time - b.startEntry.time;
    });
    return eventPairs;
  }

  function eventTypeToCssClass_(eventType) {
    return eventType.toLowerCase().replace(/_/g, '-');
  }

  /**
   * Finds all events of input type from the input list of Source Entries.
   * Returns an ordered list of start and end log entries.
   */
  function findEntryPairsFromSourceEntries_(sourceEntryList, eventType) {
    var eventPairs = [];
    for (var i = 0; i < sourceEntryList.length; ++i) {
      var sourceEntry = sourceEntryList[i];
      if (sourceEntry) {
        var entries = sourceEntry.getLogEntries();
        var matchingEventPairs = findEntryPairsByType_(entries, eventType);
        eventPairs = eventPairs.concat(matchingEventPairs);
      }
    }
    return eventPairs;
  }

  /**
   * Finds all events of input type from the input list of log entries.
   * Returns an ordered list of start and end log entries.
   */
  function findEntryPairsByType_(entries, eventType) {
    var matchingEventPairs = [];
    var startEntry = null;
    for (var i = 0; i < entries.length; ++i) {
      var currentEntry = entries[i];
      if (eventType != currentEntry.type) {
        continue;
      }
      if (currentEntry.phase == EventPhase.PHASE_BEGIN) {
        startEntry = currentEntry;
      }
      if (startEntry && currentEntry.phase == EventPhase.PHASE_END) {
        var event = {
          startEntry: startEntry,
          endEntry: currentEntry,
        };
        matchingEventPairs.push(event);
        startEntry = null;
      }
    }
    return matchingEventPairs;
  }

  /**
   * Returns an ordered list of SourceEntries that are dependencies for
   * events of the given type.
   */
  function findDependenciesOfType_(sourceEntry, eventType) {
    var sourceEntryList = [];
    if (sourceEntry) {
      var eventList = findEventsInSourceEntry_(sourceEntry, eventType);
      for (var i = 0; i < eventList.length; ++i) {
        var foundSourceEntry = findSourceEntryFromEvent_(eventList[i]);
        if (foundSourceEntry) {
          sourceEntryList.push(foundSourceEntry);
        }
      }
    }
    return sourceEntryList;
  }

  /**
   * Returns an ordered list of events from the given sourceEntry with the
   * given type.
   */
  function findEventsInSourceEntry_(sourceEntry, eventType) {
    var entries = sourceEntry.getLogEntries();
    var events = [];
    for (var i = 0; i < entries.length; ++i) {
      var currentEntry = entries[i];
      if (currentEntry.type == eventType) {
        events.push(currentEntry);
      }
    }
    return events;
  }

  /**
   * Follows the event to obtain the sourceEntry that is the source
   * dependency.
   */
  function findSourceEntryFromEvent_(event) {
    if (!('params' in event) || !('source_dependency' in event.params)) {
      return undefined;
    } else {
      var id = event.params.source_dependency.id;
      return SourceTracker.getInstance().getSourceEntry(id);
    }
  }

  return WaterfallRow;
})();
