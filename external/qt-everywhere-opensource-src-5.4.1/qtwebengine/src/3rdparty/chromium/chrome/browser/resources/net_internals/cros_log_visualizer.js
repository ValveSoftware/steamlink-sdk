// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This visualizer displays the log in a timeline graph
 *
 *   - Use HTML5 canvas
 *   - Can zoom in result by select time range
 *   - Display different levels of logs in different layers of canvases
 *
 */
var CrosLogVisualizer = (function() {
  'use strict';

  // HTML attributes of canvas
  var LOG_VISUALIZER_CANVAS_CLASS = 'cros-log-visualizer-visualizer-canvas';
  var LOG_VISUALIZER_CANVAS_WIDTH = 980;
  var LOG_VISUALIZER_CANVAS_HEIGHT = 100;

  // Special HTML classes
  var LOG_VISUALIZER_TIMELINE_ID = 'cros-log-visualizer-visualizer-timeline';
  var LOG_VISUALIZER_TIME_DISPLAY_CLASS =
      'cros-log-visualizer-visualizer-time-display';
  var LOG_VISUALIZER_RESET_BTN_ID =
      'cros-log-visualizer-visualizer-reset-btn';
  var LOG_VISUALIZER_TRACKING_LAYER_ID =
      'cros-log-visualizer-visualizer-tracking-layer';

  /**
   * Event level list
   * This list is used for intialization of canvases. And the canvas
   * with lowest priority should be created first. Hence the list is
   * sorted in decreasing order.
   */
  var LOG_EVENT_LEVEL_PRIORITY_LIST = {
    'Unknown': 4,
    'Warning': 2,
    'Info': 3,
    'Error': 1
  };

  // Color mapping of different levels
  var LOG_EVENT_COLORS_LIST = {
    'Error': '#FF99A3',
    'Warning': '#FAE5C3',
    'Info': '#C3E3FA',
    'Unknown': 'gray'
  };

  /**
   * @constructor
   */
  function CrosLogVisualizer(logVisualizer, containerID) {
    /**
     * Pass the LogVisualizer in as a reference so the visualizer can
     * synchrous with the log filter.
     */
    this.logVisualizer = logVisualizer;

    // If the data is initialized
    this.dataIntialized = false;
    // Stores all the log entries as events
    this.events = [];
    // A front layer that handles control events
    this.trackingLayer = this.createTrackingLayer();

    // References to HTML elements
    this.container = document.getElementById(containerID);
    this.timeline = this.createTimeline();
    this.timeDisplay = this.createTimeDisplay();
    this.btnReset = this.createBtnReset();
    // Canvases
    this.canvases = {};
    for (var level in LOG_EVENT_LEVEL_PRIORITY_LIST) {
      this.canvases[level] = this.createCanvas();
      this.container.appendChild(this.canvases[level]);
    }

    // Append all the elements to the container
    this.container.appendChild(this.timeline);
    this.container.appendChild(this.timeDisplay);
    this.container.appendChild(this.trackingLayer);
    this.container.appendChild(this.btnReset);

    this.container.addEventListener('webkitAnimationEnd', function() {
      this.container.classList.remove('cros-log-visualizer-flash');
    }.bind(this), false);
  }

  CrosLogVisualizer.prototype = {
    /**
     * Called during the initialization of the View. Create a overlay
     * DIV on top of the canvas that handles the mouse events
     */
    createTrackingLayer: function() {
      var trackingLayer = document.createElement('div');
      trackingLayer.setAttribute('id', LOG_VISUALIZER_TRACKING_LAYER_ID);
      trackingLayer.addEventListener('mousemove', this.onHovered_.bind(this));
      trackingLayer.addEventListener('mousedown', this.onMouseDown_.bind(this));
      trackingLayer.addEventListener('mouseup', this.onMouseUp_.bind(this));
      return trackingLayer;
    },

    /**
     * This function is called during the initialization of the view.
     * It creates the timeline that moves along with the mouse on canvas.
     * When user click, a rectangle can be dragged out to select the range
     * to zoom.
     */
    createTimeline: function() {
      var timeline = document.createElement('div');
      timeline.setAttribute('id', LOG_VISUALIZER_TIMELINE_ID);
      timeline.style.height = LOG_VISUALIZER_CANVAS_HEIGHT + 'px';
      timeline.addEventListener('mousedown', function(event) { return false; });
      return timeline;
    },

    /**
     * This function is called during the initialization of the view.
     * It creates a time display that moves with the timeline
     */
    createTimeDisplay: function() {
      var timeDisplay = document.createElement('p');
      timeDisplay.className = LOG_VISUALIZER_TIME_DISPLAY_CLASS;
      timeDisplay.style.top = LOG_VISUALIZER_CANVAS_HEIGHT + 'px';
      return timeDisplay;
    },

    /**
     * Called during the initialization of the View. Create a button that
     * resets the canvas to initial status (without zoom)
     */
    createBtnReset: function() {
      var btnReset = document.createElement('input');
      btnReset.setAttribute('type', 'button');
      btnReset.setAttribute('value', 'Reset');
      btnReset.setAttribute('id', LOG_VISUALIZER_RESET_BTN_ID);
      btnReset.addEventListener('click', this.reset.bind(this));
      return btnReset;
    },

    /**
     * Called during the initialization of the View. Create a empty canvas
     * that visualizes log when the data is ready
     */
    createCanvas: function() {
      var canvas = document.createElement('canvas');
      canvas.width = LOG_VISUALIZER_CANVAS_WIDTH;
      canvas.height = LOG_VISUALIZER_CANVAS_HEIGHT;
      canvas.className = LOG_VISUALIZER_CANVAS_CLASS;
      return canvas;
    },

    /**
     * Returns the context of corresponding canvas based on level
     */
    getContext: function(level) {
      return this.canvases[level].getContext('2d');
    },

    /**
     * Erases everything from all the canvases
     */
    clearCanvas: function() {
      for (var level in LOG_EVENT_LEVEL_PRIORITY_LIST) {
        var ctx = this.getContext(level);
        ctx.clearRect(0, 0, LOG_VISUALIZER_CANVAS_WIDTH,
            LOG_VISUALIZER_CANVAS_HEIGHT);
      }
    },

    /**
     * Initializes the parameters needed for drawing:
     *  - lower/upperBound: Time range (Events out of range will be skipped)
     *  - totalDuration: The length of time range
     *  - unitDuration: The unit time length per pixel
     */
    initialize: function() {
      if (this.events.length == 0)
        return;
      this.dragMode = false;
      this.dataIntialized = true;
      this.events.sort(this.compareTime);
      this.lowerBound = this.events[0].time;
      this.upperBound = this.events[this.events.length - 1].time;
      this.totalDuration = Math.abs(this.upperBound.getTime() -
          this.lowerBound.getTime());
      this.unitDuration = this.totalDuration / LOG_VISUALIZER_CANVAS_WIDTH;
    },

    /**
     * CSS3 fadeIn/fadeOut effects
     */
    flashEffect: function() {
      this.container.classList.add('cros-log-visualizer-flash');
    },

    /**
     * Reset the canvas to the initial time range
     * Redraw everything on the canvas
     * Fade in/out effects while redrawing
     */
    reset: function() {
      // Reset all the parameters as initial
      this.initialize();
      // Reset the visibility of the entries in the log table
      this.logVisualizer.filterLog();
      this.flashEffect();
     },

    /**
     * A wrapper function for drawing
     */
    drawEvents: function() {
      if (this.events.length == 0)
        return;
      for (var i in this.events) {
        this.drawEvent(this.events[i]);
      }
    },

    /**
     * The main function that handles drawing on the canvas.
     * Every event is represented as a vertical line.
     */
    drawEvent: function(event) {
      if (!event.visibility) {
        // Skip hidden events
        return;
      }
      var ctx = this.getContext(event.level);
      ctx.beginPath();
      // Get the x-coordinate of the line
      var startPosition = this.getPosition(event.time);
      if (startPosition != this.old) {
        this.old = startPosition;
      }
      ctx.rect(startPosition, 0, 2, LOG_VISUALIZER_CANVAS_HEIGHT);
      // Get the color of the line
      ctx.fillStyle = LOG_EVENT_COLORS_LIST[event.level];
      ctx.fill();
      ctx.closePath();
    },

    /**
     * This function is called every time the graph is zoomed.
     * It recalculates all the parameters based on the distance and direction
     * of dragging.
     */
    reCalculate: function() {
      if (this.dragDistance >= 0) {
        // if user drags to right
        this.upperBound = new Date((this.timelineLeft + this.dragDistance) *
            this.unitDuration + this.lowerBound.getTime());
        this.lowerBound = new Date(this.timelineLeft * this.unitDuration +
            this.lowerBound.getTime());
      } else {
        // if user drags to left
        this.upperBound = new Date(this.timelineLeft * this.unitDuration +
            this.lowerBound.getTime());
        this.lowerBound = new Date((this.timelineLeft + this.dragDistance) *
            this.unitDuration + this.lowerBound.getTime());
      }
      this.totalDuration = this.upperBound.getTime() -
          this.lowerBound.getTime();
      this.unitDuration = this.totalDuration / LOG_VISUALIZER_CANVAS_WIDTH;
    },

    /**
     * Check if the time of a event is out of bound
     */
    isOutOfBound: function(event) {
      return event.time.getTime() < this.lowerBound.getTime() ||
              event.time.getTime() > this.upperBound.getTime();
    },

    /**
     * This function returns the offset on x-coordinate of canvas based on
     * the time
     */
    getPosition: function(time) {
      return (time.getTime() - this.lowerBound.getTime()) / this.unitDuration;
    },

    /**
     * This function updates the events array and refresh the canvas.
     */
    updateEvents: function(newEvents) {
      this.events.length = 0;
      for (var i in newEvents) {
        this.events.push(newEvents[i]);
      }
      if (!this.dataIntialized) {
        this.initialize();
      }
      this.clearCanvas();
      this.drawEvents();
    },

    /**
     * This is a helper function that returns the time object based on the
     * offset of x-coordinate on the canvs.
     */
    getOffsetTime: function(offset) {
      return new Date(this.lowerBound.getTime() + offset * this.unitDuration);
    },

    /**
     * This function is triggered when the hovering event is detected
     * When the mouse is hovering we have two control mode:
     *  - If it is in drag mode, we need to resize the width of the timeline
     *  - If not, we need to move the timeline and time display to the
     * x-coordinate position of the mouse
     */
    onHovered_: function(event) {
      var offsetX = event.offsetX;
      if (this.lastOffsetX == offsetX) {
        // If the mouse does not move, we just skip the event
        return;
      }

      if (this.dragMode == true) {
        // If the mouse is in drag mode
        this.dragDistance = offsetX - this.timelineLeft;
        if (this.dragDistance >= 0) {
          // If the mouse is moving right
          this.timeline.style.width = this.dragDistance + 'px';
        } else {
          // If the mouse is moving left
          this.timeline.style.width = -this.dragDistance + 'px';
          this.timeline.style.left = offsetX + 'px';
        }
      } else {
        // If the mouse is not in drag mode we just move the timeline
        this.timeline.style.width = '2px';
        this.timeline.style.left = offsetX + 'px';
      }

      // update time display
      this.timeDisplay.style.left = offsetX + 'px';
      this.timeDisplay.textContent =
          this.getOffsetTime(offsetX).toTimeString().substr(0, 8);
      // update the last offset
      this.lastOffsetX = offsetX;
    },

    /**
     * This function is the handler for the onMouseDown event on the canvas
     */
    onMouseDown_: function(event) {
      // Enter drag mode which let user choose a time range to zoom in
      this.dragMode = true;
      this.timelineLeft = event.offsetX;
      // Create a duration display to indicate the duration of range.
      this.timeDurationDisplay = this.createTimeDisplay();
      this.container.appendChild(this.timeDurationDisplay);
    },

    /**
     * This function is the handler for the onMouseUp event on the canvas
     */
    onMouseUp_: function(event) {
      // Remove the duration display
      this.container.removeChild(this.timeDurationDisplay);
      // End the drag mode
      this.dragMode = false;
      // Recalculate the pamameter based on the range user select
      this.reCalculate();
      // Filter the log table and hide the entries that are not in the range
      this.logVisualizer.filterLog();
    },
  };

  return CrosLogVisualizer;
})();
