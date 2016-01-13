// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(viona): Write a README document/instructions.

/** This view displays the event waterfall. */
var WaterfallView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function WaterfallView() {
    assertFirstConstructorCall(WaterfallView);

    // Call superclass's constructor.
    superClass.call(this, WaterfallView.MAIN_BOX_ID);

    SourceTracker.getInstance().addSourceEntryObserver(this);

    // For adjusting the range of view.
    $(WaterfallView.SCALE_ID).addEventListener(
        'click', this.setStartEndTimes_.bind(this), true);

    $(WaterfallView.MAIN_BOX_ID).addEventListener(
        'mousewheel', this.scrollToZoom_.bind(this), true);

    $(WaterfallView.MAIN_BOX_ID).addEventListener(
        'scroll', this.scrollInfoTable_.bind(this), true);

    this.initializeSourceList_();

    window.onload = this.scrollInfoTable_();
  }

  WaterfallView.TAB_ID = 'tab-handle-waterfall';
  WaterfallView.TAB_NAME = 'Waterfall';
  WaterfallView.TAB_HASH = '#waterfall';

  // IDs for special HTML elements in events_waterfall_view.html.
  WaterfallView.MAIN_BOX_ID = 'waterfall-view-tab-content';
  WaterfallView.BAR_TABLE_ID = 'waterfall-view-time-bar-table';
  WaterfallView.BAR_TBODY_ID = 'waterfall-view-time-bar-tbody';
  WaterfallView.SCALE_ID = 'waterfall-view-adjust-to-window';
  WaterfallView.TIME_SCALE_HEADER_ID = 'waterfall-view-time-scale-labels';
  WaterfallView.TIME_RANGE_ID = 'waterfall-view-time-range-submit';
  WaterfallView.START_TIME_ID = 'waterfall-view-start-input';
  WaterfallView.END_TIME_ID = 'waterfall-view-end-input';
  WaterfallView.INFO_TABLE_ID = 'waterfall-view-information-table';
  WaterfallView.INFO_TBODY_ID = 'waterfall-view-information-tbody';
  WaterfallView.CONTROLS_ID = 'waterfall-view-controls';
  WaterfallView.ID_HEADER_ID = 'waterfall-view-id-header';
  WaterfallView.URL_HEADER_ID = 'waterfall-view-url-header';

  // The number of units mouse wheel deltas increase for each tick of the
  // wheel.
  var MOUSE_WHEEL_UNITS_PER_CLICK = 120;

  // Amount we zoom for one vertical tick of the mouse wheel, as a ratio.
  var MOUSE_WHEEL_ZOOM_RATE = 1.25;
  // Amount we scroll for one horizontal tick of the mouse wheel, in pixels.
  var MOUSE_WHEEL_SCROLL_RATE = MOUSE_WHEEL_UNITS_PER_CLICK;

  cr.addSingletonGetter(WaterfallView);

  WaterfallView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Creates new WaterfallRows for URL Requests when the sourceEntries are
     * updated if they do not already exist.
     * Updates pre-existing WaterfallRows that correspond to updated sources.
     */
    onSourceEntriesUpdated: function(sourceEntries) {
      for (var i = 0; i < sourceEntries.length; ++i) {
        var sourceEntry = sourceEntries[i];
        var id = sourceEntry.getSourceId();
        if (sourceEntry.getSourceType() == EventSourceType.URL_REQUEST) {
          var row = this.sourceIdToRowMap_[id];
          if (!row) {
            var newRow = new WaterfallRow(this, sourceEntry);
            this.sourceIdToRowMap_[id] = newRow;
          } else {
            row.onSourceUpdated();
          }
        }
      }
      this.scrollInfoTable_();
      this.positionBarTable_();
      this.updateTimeScale_(this.scaleFactor_);
    },

    onAllSourceEntriesDeleted: function() {
      this.initializeSourceList_();
    },

    onLoadLogFinish: function(data) {
      return true;
    },

    getScaleFactor: function() {
      return this.scaleFactor_;
    },

    setGeometry: function(left, top, width, height) {
      superClass.prototype.setGeometry.call(this, left, top, width, height);
      this.scrollInfoTable_();
    },

    show: function(isVisible) {
      superClass.prototype.show.call(this, isVisible);
      if (isVisible) {
        this.scrollInfoTable_();
      }
    },

    /**
     * Initializes the list of source entries.  If source entries are already
     * being displayed, removes them all in the process.
     */
    initializeSourceList_: function() {
      this.sourceIdToRowMap_ = {};
      $(WaterfallView.BAR_TBODY_ID).innerHTML = '';
      $(WaterfallView.INFO_TBODY_ID).innerHTML = '';
      this.scaleFactor_ = 0.1;
    },

    /**
     * Changes scroll position of the window such that horizontally, everything
     * within the specified range fits into the user's viewport.
     */
    adjustToWindow_: function(windowStart, windowEnd) {
      var waterfallLeft = $(WaterfallView.INFO_TABLE_ID).offsetWidth +
          $(WaterfallView.INFO_TABLE_ID).offsetLeft +
          $(WaterfallView.ID_HEADER_ID).offsetWidth;
      var maxWidth = $(WaterfallView.MAIN_BOX_ID).offsetWidth - waterfallLeft;
      var totalDuration = 0;
      if (windowEnd != -1) {
        totalDuration = windowEnd - windowStart;
      } else {
        for (var id in this.sourceIdToRowMap_) {
          var row = this.sourceIdToRowMap_[id];
          var rowDuration = row.getEndTicks();
          if (totalDuration < rowDuration && !row.hide) {
            totalDuration = rowDuration;
          }
        }
      }
      if (totalDuration <= 0) {
        return;
      }
      this.scaleAll_(maxWidth / totalDuration);
      $(WaterfallView.MAIN_BOX_ID).scrollLeft =
          windowStart * this.scaleFactor_;
    },

    /** Updates the time tick indicators. */
    updateTimeScale_: function(scaleFactor) {
      var timePerTick = 1;
      var minTickDistance = 20;

      $(WaterfallView.TIME_SCALE_HEADER_ID).innerHTML = '';

      // Holder provides environment to prevent wrapping.
      var timeTickRow = addNode($(WaterfallView.TIME_SCALE_HEADER_ID), 'div');
      timeTickRow.classList.add('waterfall-view-time-scale-row');

      var availableWidth = $(WaterfallView.BAR_TBODY_ID).clientWidth;
      var tickDistance = scaleFactor * timePerTick;

      while (tickDistance < minTickDistance) {
        timePerTick = timePerTick * 10;
        tickDistance = scaleFactor * timePerTick;
      }

      var tickCount = availableWidth / tickDistance;
      for (var i = 0; i < tickCount; ++i) {
        var timeCell = addNode(timeTickRow, 'div');
        setNodeWidth(timeCell, tickDistance);
        timeCell.classList.add('waterfall-view-time-scale');
        timeCell.title = i * timePerTick + ' to ' +
           (i + 1) * timePerTick + ' ms';
        // Red marker for every 5th bar.
        if (i % 5 == 0) {
          timeCell.classList.add('waterfall-view-time-scale-special');
        }
      }
    },

    /**
     * Scales all existing rows by scaleFactor.
     */
    scaleAll_: function(scaleFactor) {
      this.scaleFactor_ = scaleFactor;
      for (var id in this.sourceIdToRowMap_) {
        var row = this.sourceIdToRowMap_[id];
        row.updateRow();
      }
      this.updateTimeScale_(scaleFactor);
    },

    scrollToZoom_: function(event) {
      // To use scrolling to control zoom, hold down the alt key and scroll.
      if ('wheelDelta' in event && event.altKey) {
        event.preventDefault();
        var zoomFactor = Math.pow(MOUSE_WHEEL_ZOOM_RATE,
             event.wheelDeltaY / MOUSE_WHEEL_UNITS_PER_CLICK);

        var waterfallLeft = $(WaterfallView.ID_HEADER_ID).offsetWidth +
            $(WaterfallView.URL_HEADER_ID).offsetWidth;
        var oldCursorPosition = event.pageX +
            $(WaterfallView.MAIN_BOX_ID).scrollLeft;
        var oldCursorPositionInTable = oldCursorPosition - waterfallLeft;

        this.scaleAll_(this.scaleFactor_ * zoomFactor);

        // Shifts the view when scrolling. newScroll could be less than 0 or
        // more than the maximum scroll position, but both cases are handled
        // by the inbuilt scrollLeft implementation.
        var newScroll =
            oldCursorPositionInTable * zoomFactor - event.pageX + waterfallLeft;
        $(WaterfallView.MAIN_BOX_ID).scrollLeft = newScroll;
      }
    },

    /**
     * Positions the bar table such that it is in line with the right edge of
     * the info table.
     */
    positionBarTable_: function() {
      var offsetLeft = $(WaterfallView.INFO_TABLE_ID).offsetWidth +
          $(WaterfallView.INFO_TABLE_ID).offsetLeft;
      $(WaterfallView.BAR_TABLE_ID).style.left = offsetLeft + 'px';
    },

    /**
     * Moves the info table when the page is scrolled vertically, ensuring that
     * the correct information is displayed on the page, and that no elements
     * are blocked unnecessarily.
     */
    scrollInfoTable_: function(event) {
      $(WaterfallView.INFO_TABLE_ID).style.top =
          $(WaterfallView.MAIN_BOX_ID).offsetTop +
          $(WaterfallView.BAR_TABLE_ID).offsetTop -
          $(WaterfallView.MAIN_BOX_ID).scrollTop + 'px';

      if ($(WaterfallView.INFO_TABLE_ID).offsetHeight >
          $(WaterfallView.MAIN_BOX_ID).clientHeight) {
        var scroll = $(WaterfallView.MAIN_BOX_ID).scrollTop;
        var bottomClip =
            $(WaterfallView.MAIN_BOX_ID).clientHeight -
            $(WaterfallView.BAR_TABLE_ID).offsetTop +
            $(WaterfallView.MAIN_BOX_ID).scrollTop;
        // Clips the information table such that it does not cover the scroll
        // bars or the controls bar.
        $(WaterfallView.INFO_TABLE_ID).style.clip = 'rect(' + scroll +
            'px auto ' + bottomClip + 'px auto)';
      }
    },

    /** Parses user input, then calls adjustToWindow to shift that into view. */
    setStartEndTimes_: function() {
      var windowStart = parseInt($(WaterfallView.START_TIME_ID).value);
      var windowEnd = parseInt($(WaterfallView.END_TIME_ID).value);
      if ($(WaterfallView.END_TIME_ID).value == '') {
        windowEnd = -1;
      }
      if ($(WaterfallView.START_TIME_ID).value == '') {
        windowStart = 0;
      }
      this.adjustToWindow_(windowStart, windowEnd);
    },


  };

  return WaterfallView;
})();
