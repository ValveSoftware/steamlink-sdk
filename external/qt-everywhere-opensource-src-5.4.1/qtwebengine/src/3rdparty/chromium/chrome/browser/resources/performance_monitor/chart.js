/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('performance_monitor', function() {
  'use strict';

  /**
   * Map of available time resolutions.
   * @type {Object.<string, PerformanceMonitor.TimeResolution>}
   * @private
   */
  var TimeResolutions_ = {
    // Prior 15 min, resolution of 15 seconds.
    minutes: {id: 0, i18nKey: 'timeLastFifteenMinutes', timeSpan: 900 * 1000,
              pointResolution: 1000 * 15},

    // Prior hour, resolution of 1 minute.
    // Labels at 5 point (5 min) intervals.
    hour: {id: 1, i18nKey: 'timeLastHour', timeSpan: 3600 * 1000,
           pointResolution: 1000 * 60},

    // Prior day, resolution of 24 min.
    // Labels at 5 point (2 hour) intervals.
    day: {id: 2, i18nKey: 'timeLastDay', timeSpan: 24 * 3600 * 1000,
          pointResolution: 1000 * 60 * 24},

    // Prior week, resolution of 2.8 hours (168 min).
    // Labels at ~8.5 point (daily) intervals.
    week: {id: 3, i18nKey: 'timeLastWeek', timeSpan: 7 * 24 * 3600 * 1000,
           pointResolution: 1000 * 60 * 168},

    // Prior month (30 days), resolution of 12 hours.
    // Labels at 14 point (weekly) intervals.
    month: {id: 4, i18nKey: 'timeLastMonth', timeSpan: 30 * 24 * 3600 * 1000,
            pointResolution: 1000 * 3600 * 12},

    // Prior quarter (90 days), resolution of 36 hours.
    // Labels at ~9.3 point (fortnightly) intervals.
    quarter: {id: 5, i18nKey: 'timeLastQuarter',
              timeSpan: 90 * 24 * 3600 * 1000,
              pointResolution: 1000 * 3600 * 36},
  };

  /**
   * Map of available date formats in Flot-style format strings.
   * @type {Object.<string, string>}
   * @private
   */
  var TimeFormats_ = {
    time: '%h:%M %p',
    monthDayTime: '%b %d<br/>%h:%M %p',
    monthDay: '%b %d',
    yearMonthDay: '%y %b %d',
  };

  /*
   * Table of colors to use for metrics and events. Basically boxing the
   * colorwheel, but leaving out yellows and fully saturated colors.
   * @type {Array.<string>}
   * @private
   */
  var ColorTable_ = [
    'rgb(255, 128, 128)', 'rgb(128, 255, 128)', 'rgb(128, 128, 255)',
    'rgb(128, 255, 255)', 'rgb(255, 128, 255)', // No bright yellow
    'rgb(255,  64,  64)', 'rgb( 64, 255,  64)', 'rgb( 64,  64, 255)',
    'rgb( 64, 255, 255)', 'rgb(255,  64, 255)', // No medium yellow either
    'rgb(128,  64,  64)', 'rgb( 64, 128,  64)', 'rgb( 64,  64, 128)',
    'rgb( 64, 128, 128)', 'rgb(128,  64, 128)', 'rgb(128, 128,  64)'
  ];

  /*
   * Offset, in ms, by which to subtract to convert GMT to local time.
   * @type {number}
   * @private
   */
  var timezoneOffset_ = new Date().getTimezoneOffset() * 60000;

  /*
   * Additional range multiplier to ensure that points don't hit the top of
   * the graph.
   * @type {number}
   * @private
   */
  var yAxisMargin_ = 1.05;

  /*
   * Number of time resolution periods to wait between automated update of
   * graphs.
   * @type {number}
   * @private
   */
  var intervalMultiple_ = 2;

  /*
   * Number of milliseconds to wait before deciding that the most recent
   * resize event is not going to be followed immediately by another, and
   * thus needs handling.
   * @type {number}
   * @private
   */
  var resizeDelay_ = 500;

  /*
   * The value of the 'No Aggregation' option enum (AGGREGATION_METHOD_NONE) on
   * the C++ side. We use this to warn the user that selecting this aggregation
   * option will be slow.
   */
  var aggregationMethodNone = 0;

  /*
   * The value of the default aggregation option, 'Median Aggregation'
   * (AGGREGATION_METHOD_MEDIAN), on the C++ side.
   */
  var aggregationMethodMedian = 1;

  /** @constructor */
  function PerformanceMonitor() {
    this.__proto__ = PerformanceMonitor.prototype;

    /** Information regarding a certain time resolution option, including an
     *  enumerative id, a readable name, the timespan in milliseconds prior to
     *  |now|, data point resolution in milliseconds, and time-label frequency
     *  in data points per label.
     *  @typedef {{
     *    id: number,
     *    name: string,
     *    timeSpan: number,
     *    pointResolution: number,
     *    labelEvery: number,
     *  }}
     */
    PerformanceMonitor.TimeResolution;

    /**
     * Detailed information on a metric in the UI. |metricId| is a unique
     * identifying number for the metric, provided by the webui, and assumed to
     * be densely populated. |description| is a localized string description
     * suitable for mouseover on the metric. |category| corresponds to a
     * category object to which the metric belongs (see |metricCategoryMap_|).
     * |color| is the color in which the metric is displayed on the graphs.
     * |maxValue| is a value by which to scale the y-axis, in order to avoid
     * constant resizing to fit the present data. |checkbox| is the HTML element
     * for the checkbox which toggles the metric's display. |enabled| indicates
     * whether or not the metric is being actively displayed. |data| is the
     * collection of data for the metric.
     *
     * For |data|, the inner-most array represents a point in a pair of numbers,
     * representing time and value (this will always be of length 2). The
     * array above is the collection of points within a series, which is an
     * interval for which PerformanceMonitor was active. The outer-most array
     * is the collection of these series.
     *
     * @typedef {{
     *   metricId: number,
     *   description: string,
     *   category: !Object,
     *   color: string,
     *   maxValue: number,
     *   checkbox: HTMLElement,
     *   enabled: boolean,
     *   data: ?Array.<Array<Array<number> > >
     * }}
     */
    PerformanceMonitor.MetricDetails;

    /**
     * Similar data for events as for metrics, though no y-axis info is needed
     * since events are simply labeled markers at X locations.
     *
     * The |data| field follows a special rule not describable in
     * JSDoc: Aside from the |time| key, each event type has varying other
     * properties, with unknown key names, which properties must still be
     * displayed. Such properties always have value of form
     * {label: 'some label', value: 'some value'}, with label and value
     * internationalized.
     *
     * @typedef {{
     *   eventId: number,
     *   name: string,
     *   popupTitle: string,
     *   description: string,
     *   color: string,
     *   checkbox: HTMLElement,
     *   enabled: boolean
     *   data: ?Array.<{time: number}>
     * }}
     */
    PerformanceMonitor.EventDetails;

    /**
     * The collection of divs that compose a chart on the UI, plus the metricIds
     * of any metric which should be shown on the chart (whether the metric is
     * enabled or not). The |mainDiv| is the full element, under which all other
     * divs are nested. The |grid| is the div into which the |plot| (which is
     * the core of the graph, including the axis, gridlines, dataseries, etc)
     * goes. The |yaxisLabel| is nested under the mainDiv, and shows the units
     * for the chart.
     *
     * @typedef {{
     *   mainDiv: HTMLDivElement,
     *   grid: HTMLDivElement,
     *   plot: HTMLDivElement,
     *   yaxisLabel: HTMLDivElement,
     *   metricIds: ?Array.<number>
     */
    PerformanceMonitor.Chart;

    /**
     * The time range which we are currently viewing, with the start and end of
     * the range, the TimeResolution, and an appropriate for display (this
     * format is the string structure which Flot expects for its setting).
     * @typedef {{
     * @type {{
     *   start: number,
     *   end: number,
     *   resolution: PerformanceMonitor.TimeResolution
     *   format: string
     * }}
     * @private
     */
    this.range_ = { 'start': 0, 'end': 0, 'resolution': undefined };

    /**
     * The map containing the available TimeResolutions and the radio button to
     * which each corresponds. The key is the id field from the TimeResolution
     * object.
     * @type {Object.<string, {
     *   option: PerformanceMonitor.TimeResolution,
     *   element: HTMLElement
     * }>}
     * @private
     */
    this.timeResolutionRadioMap_ = {};

    /**
     * The map containing the available Aggregation Methods and the radio button
     * to which each corresponds. The different methods are retrieved from the
     * WebUI, and the information about the method is stored in the 'option'
     * field. The key to the map is the id of the aggregation method.
     *
     * @type {Object.<string, {
     *   option: {
     *     id: number,
     *     name: string,
     *     description: string,
     *   },
     *   element: HTMLElement
     * }>}
     * @private
     */
    this.aggregationRadioMap_ = {};

    /**
     * Metrics fall into categories that have common units and thus may
     * share a common graph, or share y-axes within a multi-y-axis graph.
     * Each category has a unique identifying metricCategoryId; a localized
     * name, mouseover description, and unit; and an array of all the metrics
     * which are in this category. The key is |metricCategoryId|.
     *
     * @type {Object.<string, {
     *   metricCategoryId: number,
     *   name: string,
     *   description: string,
     *   unit: string,
     *   details: Array.<{!PerformanceMonitor.MetricDetails}>,
     * }>}
     * @private
     */
    this.metricCategoryMap_ = {};

    /**
     * Comprehensive map from metricId to MetricDetails.
     * @type {Object.<string, {PerformanceMonitor.MetricDetails}>}
     * @private
     */
    this.metricDetailsMap_ = {};

    /**
     * Events fall into categories just like metrics, above. This category
     * grouping is not as important as that for metrics, since events
     * needn't share maxima, y-axes, nor units, and since events appear on
     * all charts. But grouping of event categories in the event-selection
     * UI is still useful. The key is the id of the event category.
     *
     * @type {Object.<string, {
     *   eventCategoryId: number,
     *   name: string,
     *   description: string,
     *   details: !Array.<!PerformanceMonitor.EventDetails>,
     * }>}
     * @private
     */
    this.eventCategoryMap_ = {};

    /**
     * Comprehensive map from eventId to EventDetails.
     * @type {Object.<string, {PerformanceMonitor.EventDetails}>}
     * @private
     */
    this.eventDetailsMap_ = {};

    /**
     * Time periods in which the browser was active and collecting metrics
     * and events.
     * @type {!Array.<{start: number, end: number}>}
     * @private
     */
    this.intervals_ = [];

    /**
     * The record of all the warnings which are currently active (or empty if no
     * warnings are being displayed).
     * @type {!Array.<string>}
     * @private
     */
    this.activeWarnings_ = [];

    /**
     * Handle of timer interval function used to update charts
     * @type {Object}
     * @private
     */
    this.updateTimer_ = null;

    /**
     * Handle of timer interval function used to check for resizes. Nonnull
     * only when resize events are coming steadily.
     * @type {Object}
     * @private
     */
    this.resizeTimer_ = null;

    /**
     * The status of all calls for data, stored in order to keep track of the
     * internal state. This stores an attribute for each type of repeated data
     * call (for now, only metrics and events), which will be true if we are
     * awaiting data and false otherwise.
     * @type {Object.<string, boolean>}
     * @private
     */
    this.awaitingDataCalls_ = {};

    /**
     * The progress into the initialization process. This must be stored, since
     * certain tasks must be performed in a specific order which cannot be
     * statically determined. Mainly, we must not request any data until the
     * metrics, events, aggregation method, and time range have all been set.
     * This object contains an attribute for each stage of the initialization
     * process, which is set to true if the stage has been completed.
     * @type {Object.<string, boolean>}
     * @private
     */
    this.initProgress_ = { 'aggregation': false,
                           'events': false,
                           'metrics': false,
                           'timeRange': false };

    /**
     * All PerformanceMonitor.Chart objects available in the display, whether
     * hidden or visible.
     * @type {Array.<PerformanceMonitor.Chart>}
     * @private
     */
    this.charts_ = [];

    this.setupStaticControlPanelFeatures_();
    chrome.send('getFlagEnabled');
    chrome.send('getAggregationTypes');
    chrome.send('getEventTypes');
    chrome.send('getMetricTypes');
  }

  PerformanceMonitor.prototype = {
    /**
     * Display the appropriate warning at the top of the page.
     * @param {string} warningId the id of the HTML element with the warning
     *     to display; this does not include the '#'.
     */
    showWarning: function(warningId) {
      if (this.activeWarnings_.indexOf(warningId) != -1)
        return;

      if (this.activeWarnings_.length == 0)
        $('#warnings-box')[0].style.display = 'block';
      $('#' + warningId)[0].style.display = 'block';
      this.activeWarnings_.push(warningId);
    },

    /**
     * Hide the warning, and, if that was the only warning showing, the entire
     * warnings box.
     * @param {string} warningId the id of the HTML element with the warning
     *     to display; this does not include the '#'.
     */
    hideWarning: function(warningId) {
      var index = this.activeWarnings_.indexOf(warningId);
      if (index == -1)
        return;
      $('#' + warningId)[0].style.display = 'none';
      this.activeWarnings_.splice(index, 1);

      if (this.activeWarnings_.length == 0)
        $('#warnings-box')[0].style.display = 'none';
    },

    /**
     * Receive an indication of whether or not the kPerformanceMonitorGathering
     * flag has been enabled and, if not, warn the user of such.
     * @param {boolean} flagEnabled indicates whether or not the flag has been
     *     enabled.
     */
    getFlagEnabledCallback: function(flagEnabled) {
      if (!flagEnabled)
        this.showWarning('flag-not-enabled-warning');
    },

    /**
     * Return true if we are not awaiting any returning data calls, and false
     * otherwise.
     * @return {boolean} The value indicating whether or not we are actively
     *     fetching data.
     */
    fetchingData_: function() {
      return this.awaitingDataCalls_.metrics == true ||
             this.awaitingDataCalls_.events == true;
    },

    /**
     * Return true if the main steps of initialization prior to the first draw
     * are complete, and false otherwise.
     * @return {boolean} The value indicating whether or not the initialization
     *     process has finished.
     */
    isInitialized_: function() {
      return this.initProgress_.aggregation == true &&
             this.initProgress_.events == true &&
             this.initProgress_.metrics == true &&
             this.initProgress_.timeRange == true;
    },

    /**
     * Refresh all data areas.
     */
    refreshAll: function() {
      this.refreshMetrics();
      this.refreshEvents();
    },

    /**
     * Receive a list of all the aggregation methods. Populate
     * |this.aggregationRadioMap_| to reflect said list. Create the section of
     * radio buttons for the aggregation methods, and choose the first method
     * by default.
     * @param {Array<{
     *   id: number,
     *   name: string,
     *   description: string
     * }>} methods All aggregation methods needing radio buttons.
     */
    getAggregationTypesCallback: function(methods) {
      methods.forEach(function(method) {
        this.aggregationRadioMap_[method.id] = { 'option': method };
      }, this);

      this.setupRadioButtons_($('#choose-aggregation')[0],
                              this.aggregationRadioMap_,
                              this.setAggregationMethod,
                              aggregationMethodMedian,
                              'aggregation-methods');
      this.setAggregationMethod(aggregationMethodMedian);
      this.initProgress_.aggregation = true;
      if (this.isInitialized_())
        this.refreshAll();
    },

    /**
     * Receive a list of all metric categories, each with its corresponding
     * list of metric details. Populate |this.metricCategoryMap_| and
     * |this.metricDetailsMap_| to reflect said list. Reconfigure the
     * checkbox set for metric selection.
     * @param {Array.<{
     *   metricCategoryId: number,
     *   name: string,
     *   unit: string,
     *   description: string,
     *   details: Array.<{
     *     metricId: number,
     *     name: string,
     *     description: string
     *   }>
     * }>} categories All metric categories needing charts and checkboxes.
     */
    getMetricTypesCallback: function(categories) {
      categories.forEach(function(category) {
        this.addCategoryChart_(category);
        this.metricCategoryMap_[category.metricCategoryId] = category;

        category.details.forEach(function(metric) {
          metric.color = ColorTable_[metric.metricId % ColorTable_.length];
          metric.maxValue = 1;
          metric.divs = [];
          metric.data = null;
          metric.category = category;
          this.metricDetailsMap_[metric.metricId] = metric;
        }, this);
      }, this);

      this.setupCheckboxes_($('#choose-metrics')[0],
          this.metricCategoryMap_, 'metricId', this.addMetric, this.dropMetric);

      for (var metric in this.metricDetailsMap_) {
        this.metricDetailsMap_[metric].checkbox.checked = true;
        this.metricDetailsMap_[metric].enabled = true;
      }

      this.initProgress_.metrics = true;
      if (this.isInitialized_())
        this.refreshAll();
    },

    /**
     * Receive a list of all event categories, each with its correspoinding
     * list of event details. Populate |this.eventCategoryMap_| and
     * |this.eventDetailsMap| to reflect said list. Reconfigure the
     * checkbox set for event selection.
     * @param {Array.<{
     *   eventCategoryId: number,
     *   name: string,
     *   description: string,
     *   details: Array.<{
     *     eventId: number,
     *     name: string,
     *     description: string
     *   }>
     * }>} categories All event categories needing charts and checkboxes.
     */
    getEventTypesCallback: function(categories) {
      categories.forEach(function(category) {
        this.eventCategoryMap_[category.eventCategoryId] = category;

        category.details.forEach(function(event) {
          event.color = ColorTable_[event.eventId % ColorTable_.length];
          event.divs = [];
          event.data = null;
          this.eventDetailsMap_[event.eventId] = event;
        }, this);
      }, this);

      this.setupCheckboxes_($('#choose-events')[0], this.eventCategoryMap_,
          'eventId', this.addEventType, this.dropEventType);

      this.initProgress_.events = true;
      if (this.isInitialized_())
        this.refreshAll();
    },

    /**
     * Set up the aspects of the control panel which are not dependent upon the
     * information retrieved from PerformanceMonitor's database; this includes
     * the Time Resolutions and Aggregation Methods radio sections.
     * @private
     */
    setupStaticControlPanelFeatures_: function() {
      // Initialize the options in the |timeResolutionRadioMap_| and set the
      // localized names for the time resolutions.
      for (var key in TimeResolutions_) {
        var resolution = TimeResolutions_[key];
        this.timeResolutionRadioMap_[resolution.id] = { 'option': resolution };
        resolution.name = loadTimeData.getString(resolution.i18nKey);
      }

      // Setup the Time Resolution radio buttons, and select the default option
      // of minutes (finer resolution in order to ensure that the user sees
      // something at startup).
      this.setupRadioButtons_($('#choose-time-range')[0],
                              this.timeResolutionRadioMap_,
                              this.changeTimeResolution_,
                              TimeResolutions_.minutes.id,
                              'time-resolutions');

      // Set the default selection to 'Minutes' and set the time range.
      this.setTimeRange(TimeResolutions_.minutes,
                        Date.now(),
                        true);  // Auto-refresh the chart.

      var forwardButton = $('#forward-time')[0];
      forwardButton.addEventListener('click', this.forwardTime.bind(this));
      var backButton = $('#back-time')[0];
      backButton.addEventListener('click', this.backTime.bind(this));

      this.initProgress_.timeRange = true;
      if (this.isInitialized_())
        this.refreshAll();
    },

    /**
     * Change the current time resolution. The visible range will stay centered
     * around the current center unless the latest edge crosses now(), in which
     * case it will be pinned there and start auto-updating.
     * @param {number} mapId the index into the |timeResolutionRadioMap_| of the
     *     selected resolution.
     */
    changeTimeResolution_: function(mapId) {
      var newEnd;
      var now = Date.now();
      var newResolution = this.timeResolutionRadioMap_[mapId].option;

      // If we are updating the timer, then we know that we are already ending
      // at the perceived current time (which may be different than the actual
      // current time, since we don't update continuously).
      newEnd = this.updateTimer_ ? now :
          Math.min(now, this.range_.end + (newResolution.timeSpan -
              this.range_.resolution.timeSpan) / 2);

      this.setTimeRange(newResolution, newEnd, newEnd == now);
    },

    /**
     * Generalized function to create checkboxes for either events
     * or metrics, given a |div| into which to put the checkboxes, and a
     * |optionCategoryMap| describing the checkbox structure.
     *
     * For instance, |optionCategoryMap| might be metricCategoryMap_, with
     * contents thus:
     *
     * optionCategoryMap : {
     *   1: {
     *     name: 'CPU',
     *     details: [
     *       {
     *         metricId: 1,
     *         name: 'CPU Usage',
     *         description:
     *             'The combined CPU usage of all processes related to Chrome',
     *         color: 'rgb(255, 128, 128)'
     *       }
     *     ],
     *   2: {
     *     name : 'Memory',
     *     details: [
     *       {
     *         metricId: 2,
     *         name: 'Private Memory Usage',
     *         description:
     *             'The combined private memory usage of all processes related
     *             to Chrome',
     *         color: 'rgb(128, 255, 128)'
     *       },
     *       {
     *         metricId: 3,
     *         name: 'Shared Memory Usage',
     *         description:
     *             'The combined shared memory usage of all processes related
     *             to Chrome',
     *         color: 'rgb(128, 128, 255)'
     *       }
     *     ]
     *  }
     *
     * and we would call setupCheckboxes_ thus:
     *
     * this.setupCheckboxes_(<parent div>, this.metricCategoryMap_, 'metricId',
     *     this.addMetric, this.dropMetric);
     *
     * MetricCategoryMap_'s values each have a |name| and |details| property.
     * SetupCheckboxes_ creates one major header for each such value, with title
     * given by the |name| field. Under each major header are checkboxes,
     * one for each element in the |details| property. The checkbox titles
     * come from the |name| property of each |details| object,
     * and they each have an associated colored icon matching the |color|
     * property of the details object.
     *
     * So, for the example given, the generated HTML looks thus:
     *
     * <div>
     *   <h3 class="category-heading">CPU</h3>
     *   <div class="checkbox-group">
     *     <div>
     *       <label class="input-label" title=
     *           "The combined CPU usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>CPU</span>
     *       </label>
     *     </div>
     *   </div>
     * </div>
     * <div>
     *   <h3 class="category-heading">Memory</h3>
     *   <div class="checkbox-group">
     *     <div>
     *       <label class="input-label" title= "The combined private memory \
     *           usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>Private Memory</span>
     *       </label>
     *     </div>
     *     <div>
     *       <label class="input-label" title= "The combined shared memory \
     *           usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>Shared Memory</span>
     *       </label>
     *     </div>
     *   </div>
     * </div>
     *
     * The checkboxes for each details object call addMetric or
     * dropMetric as they are checked and unchecked, passing the relevant
     * |metricId| value. Parameter 'metricId' identifies key |metricId| as the
     * identifying property to pass to the methods. So, for instance, checking
     * the CPU Usage box results in a call to this.addMetric(1), since
     * metricCategoryMap_[1].details[0].metricId == 1.
     *
     * In general, |optionCategoryMap| must have values that each include
     * a property |name|, and a property |details|. The |details| value must
     * be an array of objects that in turn each have an identifying property
     * with key given by parameter |idKey|, plus a property |name| and a
     * property |color|.
     *
     * @param {!HTMLDivElement} div A <div> into which to put checkboxes.
     * @param {!Object} optionCategoryMap A map of metric/event categories.
     * @param {string} idKey The key of the id property.
     * @param {!function(this:Controller, Object)} check
     *     The function to select an entry (metric or event).
     * @param {!function(this:Controller, Object)} uncheck
     *     The function to deselect an entry (metric or event).
     * @private
     */
    setupCheckboxes_: function(div, optionCategoryMap, idKey, check, uncheck) {
      var categoryTemplate = $('#category-template')[0];
      var checkboxTemplate = $('#checkbox-template')[0];

      for (var c in optionCategoryMap) {
        var category = optionCategoryMap[c];
        var template = categoryTemplate.cloneNode(true);
        template.id = '';

        var heading = template.querySelector('.category-heading');
        heading.innerText = category.name;
        heading.title = category.description;

        var checkboxGroup = template.querySelector('.checkbox-group');
        category.details.forEach(function(details) {
          var checkbox = checkboxTemplate.cloneNode(true);
          checkbox.id = '';
          var input = checkbox.querySelector('input');

          details.checkbox = input;
          input.checked = false;
          input.option = details[idKey];
          input.addEventListener('change', function(e) {
            (e.target.checked ? check : uncheck).call(this, e.target.option);
          }.bind(this));

          checkbox.querySelector('span').innerText = details.name;
          checkbox.querySelector('.input-label').title = details.description;

          checkboxGroup.appendChild(checkbox);
        }, this);

        div.appendChild(template);
      }
    },

    /**
     * Generalized function to create radio buttons in a collection of
     * |collectionName|, given a |div| into which the radio buttons are placed
     * and a |optionMap| describing the radio buttons' options.
     *
     * optionMaps have two guaranteed fields - 'option' and 'element'. The
     * 'option' field corresponds to the item which the radio button will be
     * representing (e.g., a particular aggregation method).
     *   - Each 'option' is guaranteed to have a 'value', a 'name', and a
     *     'description'. 'Value' holds the id of the option, while 'name' and
     *     'description' are internationalized strings for the radio button's
     *     content.
     *   - 'Element' is the field devoted to the HTMLElement for the radio
     *     button corresponding to that entry; this will be set in this
     *     function.
     *
     * Assume that |optionMap| is |aggregationRadioMap_|, as follows:
     * optionMap: {
     *   0: {
     *     option: {
     *       id: 0
     *       name: 'Median'
     *       description: 'Aggregate using median calculations to reduce
     *           noisiness in reporting'
     *     },
     *     element: null
     *   },
     *   1: {
     *     option: {
     *       id: 1
     *       name: 'Mean'
     *       description: 'Aggregate using mean calculations for the most
     *           accurate average in reporting'
     *     },
     *     element: null
     *   }
     * }
     *
     * and we would call setupRadioButtons_ with:
     * this.setupRadioButtons_(<parent_div>, this.aggregationRadioMap_,
     *     this.setAggregationMethod, 0, 'aggregation-methods');
     *
     * The resultant HTML would be:
     * <div class="radio">
     *   <label class="input-label" title="Aggregate using median \
     *       calculations to reduce noisiness in reporting">
     *     <input type="radio" name="aggregation-methods" value=0>
     *     <span>Median</span>
     *   </label>
     * </div>
     * <div class="radio">
     *   <label class="input-label" title="Aggregate using mean \
     *       calculations for the most accurate average in reporting">
     *     <input type="radio" name="aggregation-methods" value=1>
     *     <span>Mean</span>
     *   </label>
     * </div>
     *
     * If a radio button is selected, |onSelect| is called with the radio
     * button's value. The |defaultKey| is used to choose which radio button
     * to select at startup; the |onSelect| method is not called on this
     * selection.
     *
     * @param {!HTMLDivElement} div A <div> into which we place the radios.
     * @param {!Object} optionMap A map containing the radio button information.
     * @param {!function(this:Controller, Object)} onSelect
     *     The function called when a radio is selected.
     * @param {string} defaultKey The key to the radio which should be selected
     *     initially.
     * @param {string} collectionName The name of the radio button collection.
     * @private
     */
    setupRadioButtons_: function(div,
                                 optionMap,
                                 onSelect,
                                 defaultKey,
                                 collectionName) {
      var radioTemplate = $('#radio-template')[0];
      for (var key in optionMap) {
        var entry = optionMap[key];
        var radio = radioTemplate.cloneNode(true);
        radio.id = '';
        var input = radio.querySelector('input');

        input.name = collectionName;
        input.enumerator = entry.option.id;
        input.option = entry;
        radio.querySelector('span').innerText = entry.option.name;
        if (entry.option.description != undefined)
          radio.querySelector('.input-label').title = entry.option.description;
        div.appendChild(radio);
        entry.element = input;
      }

      optionMap[defaultKey].element.click();

      div.addEventListener('click', function(e) {
        if (!e.target.webkitMatchesSelector('input[type="radio"]'))
          return;

        onSelect.call(this, e.target.enumerator);
      }.bind(this));
    },

    /**
     * Add a new chart for |category|, making it initially hidden,
     * with no metrics displayed in it.
     * @param {!Object} category The metric category for which to create
     *     the chart. Category is a value from metricCategoryMap_.
     * @private
     */
    addCategoryChart_: function(category) {
      var chartParent = $('#charts')[0];
      var mainDiv = $('#chart-template')[0].cloneNode(true);
      mainDiv.id = '';

      var yaxisLabel = mainDiv.querySelector('h4');
      yaxisLabel.innerText = category.unit;

      // Rotation is weird in html. The length of the text affects the x-axis
      // placement of the label. We shift it back appropriately.
      var width = -1 * (yaxisLabel.offsetWidth / 2) + 20;
      var widthString = width.toString() + 'px';
      yaxisLabel.style.webkitMarginStart = widthString;

      var grid = mainDiv.querySelector('.grid');

      mainDiv.hidden = true;
      chartParent.appendChild(mainDiv);

      grid.hovers = [];

      // Set the various fields for the PerformanceMonitor.Chart object, and
      // add the new object to |charts_|.
      var chart = {};
      chart.mainDiv = mainDiv;
      chart.yaxisLabel = yaxisLabel;
      chart.grid = grid;
      chart.metricIds = [];

      category.details.forEach(function(details) {
        chart.metricIds.push(details.metricId);
      });

      this.charts_.push(chart);

      // Receive hover events from Flot.
      // Attached to chart will be properties 'hovers', a list of {x, div}
      // pairs. As pos events arrive, check each hover to see if it should
      // be hidden or made visible.
      $(grid).bind('plothover', function(event, pos, item) {
        var tolerance = this.range_.resolution.pointResolution;

        grid.hovers.forEach(function(hover) {
          hover.div.hidden = hover.x < pos.x - tolerance ||
              hover.x > pos.x + tolerance;
        });

      }.bind(this));

      $(window).resize(function() {
        if (this.resizeTimer_ != null)
          clearTimeout(this.resizeTimer_);
        this.resizeTimer_ = setTimeout(this.checkResize_.bind(this),
            resizeDelay_);
      }.bind(this));
    },

    /**
     * |resizeDelay_| ms have elapsed since the last resize event, and the timer
     * for redrawing has triggered. Clear it, and redraw all the charts.
     * @private
     */
    checkResize_: function() {
      clearTimeout(this.resizeTimer_);
      this.resizeTimer_ = null;

      this.drawCharts();
    },

    /**
     * Set the time range for which to display metrics and events. For
     * now, the time range always ends at 'now', but future implementations
     * may allow time ranges not so anchored. Also set the format string for
     * Flot.
     *
     * @param {TimeResolution} resolution
     *     The time resolution at which to display the data.
     * @param {number} end Ending time, in ms since epoch, to which to
     *     set the new time range.
     * @param {boolean} autoRefresh Indicates whether we should restart the
     *     range-update timer.
     */
    setTimeRange: function(resolution, end, autoRefresh) {
      // If we have a timer and we are no longer updating, or if we need a timer
      // for a different resolution, disable the current timer.
      if (this.updateTimer_ &&
              (this.range_.resolution != resolution || !autoRefresh)) {
        clearInterval(this.updateTimer_);
        this.updateTimer_ = null;
      }

      if (autoRefresh && !this.updateTimer_) {
        this.updateTimer_ = setInterval(
            this.forwardTime.bind(this),
            intervalMultiple_ * resolution.pointResolution);
      }

      this.range_.resolution = resolution;
      this.range_.end = Math.floor(end / resolution.pointResolution) *
          resolution.pointResolution;
      this.range_.start = this.range_.end - resolution.timeSpan;
      this.setTimeFormat_();

      if (this.isInitialized_())
        this.refreshAll();
    },

    /**
     * Set the format string for Flot. For time formats, we display the time
     * if we are showing data only for the current day; we display the month,
     * day, and time if we are showing data for multiple days at a fine
     * resolution; we display the month and day if we are showing data for
     * multiple days within the same year at course resolution; and we display
     * the year, month, and day if we are showing data for multiple years.
     * @private
     */
    setTimeFormat_: function() {
      // If the range is set to a week or less, then we will need to show times.
      if (this.range_.resolution.id <= TimeResolutions_['week'].id) {
        var dayStart = new Date();
        dayStart.setHours(0);
        dayStart.setMinutes(0);

        if (this.range_.start >= dayStart.getTime())
          this.range_.format = TimeFormats_['time'];
        else
          this.range_.format = TimeFormats_['monthDayTime'];
      } else {
        var yearStart = new Date();
        yearStart.setMonth(0);
        yearStart.setDate(0);

        if (this.range_.start >= yearStart.getTime())
          this.range_.format = TimeFormats_['monthDay'];
        else
          this.range_.format = TimeFormats_['yearMonthDay'];
      }
    },

    /**
     * Back up the time range by 1/2 of its current span, and cause chart
     * redraws.
     */
    backTime: function() {
      this.setTimeRange(this.range_.resolution,
                        this.range_.end - this.range_.resolution.timeSpan / 2,
                        false);
    },

    /**
     * Advance the time range by 1/2 of its current span, or up to the point
     * where it ends at the present time, whichever is less.
     */
    forwardTime: function() {
      var now = Date.now();
      var newEnd =
          Math.min(now, this.range_.end + this.range_.resolution.timeSpan / 2);

      this.setTimeRange(this.range_.resolution, newEnd, newEnd == now);
    },

    /**
     * Set the aggregation method.
     * @param {number} methodId The id of the aggregation method.
     */
    setAggregationMethod: function(methodId) {
      if (methodId != aggregationMethodNone)
        this.hideWarning('no-aggregation-warning');
      else
        this.showWarning('no-aggregation-warning');

      this.aggregationMethod = methodId;
      if (this.isInitialized_())
        this.refreshMetrics();
    },

    /**
     * Add a new metric to the display, fetching its data and triggering a
     * chart redraw.
     * @param {number} metricId The id of the metric to start displaying.
     */
    addMetric: function(metricId) {
      var metric = this.metricDetailsMap_[metricId];
      metric.enabled = true;
      this.refreshMetrics();
    },

    /**
     * Remove a metric from its homechart, triggering a chart redraw.
     * @param {number} metricId The metric to stop displaying.
     */
    dropMetric: function(metricId) {
      var metric = this.metricDetailsMap_[metricId];
      metric.enabled = false;
      this.drawCharts();
    },

    /**
     * Refresh all metrics which are active on the graph in one call to the
     * webui. Results will be returned in getMetricsCallback().
     */
    refreshMetrics: function() {
      var metrics = [];

      for (var metric in this.metricDetailsMap_) {
        if (this.metricDetailsMap_[metric].enabled)
          metrics.push(this.metricDetailsMap_[metric].metricId);
      }

      if (!metrics.length)
        return;

      this.awaitingDataCalls_.metrics = true;
      chrome.send('getMetrics',
                  [metrics,
                   this.range_.start, this.range_.end,
                   this.range_.resolution.pointResolution,
                   this.aggregationMethod]);
    },

    /**
     * The callback from refreshing the metrics. The resulting metrics will be
     * returned in a list, containing for each active metric a list of data
     * point series, representing the time periods for which PerformanceMonitor
     * was active. These data will be in sorted order, and will be aggregated
     * according to |aggregationMethod_|. These data are put into a Flot-style
     * series, with each point stored in an array of length 2, comprised of the
     * time and the value of the point.
     * @param Array<{
     *   metricId: number,
     *   data: Array<{time: number, value: number}>,
     *   maxValue: number
     * }> results The data for the requested metrics.
     */
    getMetricsCallback: function(results) {
      results.forEach(function(metric) {
        var metricDetails = this.metricDetailsMap_[metric.metricId];

        metricDetails.data = [];

        // Each data series sent back represents a interval for which
        // PerformanceMonitor was active. Iterate through the points of each
        // series, converting them to Flot standard (an array of time, value
        // pairs).
        metric.metrics.forEach(function(series) {
          var seriesData = [];
          series.forEach(function(point) {
            seriesData.push([point.time - timezoneOffset_, point.value]);
          });
          metricDetails.data.push(seriesData);
        });

        metricDetails.maxValue = Math.max(metricDetails.maxValue,
                                          metric.maxValue);
      }, this);

      this.awaitingDataCalls_.metrics = false;
      this.drawCharts();
    },

    /**
     * Add a new event to the display, fetching its data and triggering a
     * redraw.
     * @param {number} eventType The type of event to start displaying.
     */
    addEventType: function(eventId) {
      this.eventDetailsMap_[eventId].enabled = true;
      this.refreshEvents();
    },

    /*
     * Remove an event from the display, triggering a redraw.
     * @param {number} eventId The type of event to stop displaying.
     */
    dropEventType: function(eventId) {
      this.eventDetailsMap_[eventId].enabled = false;
      this.drawCharts();
    },

    /**
     * Refresh all events which are active on the graph in one call to the
     * webui. Results will be returned in getEventsCallback().
     */
    refreshEvents: function() {
      var events = [];
      for (var eventType in this.eventDetailsMap_) {
        if (this.eventDetailsMap_[eventType].enabled)
          events.push(this.eventDetailsMap_[eventType].eventId);
      }
      if (!events.length)
        return;

      this.awaitingDataCalls_.events = true;
      chrome.send('getEvents', [events, this.range_.start, this.range_.end]);
    },

    /**
     * The callback from refreshing events. Resulting events are stored in a
     * list object, which contains for each event type requested a series
     * of event points. Each event point contains a time and an arbitrary list
     * of additional properties to be displayed as a tooltip message for the
     * event.
     * @param Array.<{
     *   eventId: number,
     *   Array.<{time: number}>
     * }> results The collection of events for the requested types.
     */
    getEventsCallback: function(results) {
      results.forEach(function(eventSet) {
        var eventType = this.eventDetailsMap_[eventSet.eventId];

        eventSet.events.forEach(function(eventData) {
          eventData.time -= timezoneOffset_;
        });
        eventType.data = eventSet.events;
      }, this);

      this.awaitingDataCalls_.events = false;
      this.drawCharts();
    },

    /**
     * Create and return an array of 'markings' (per Flot), representing
     * vertical lines at the event time, in the event's color. Also add
     * (not per Flot) a |popupTitle| property to each, to be used for
     * labeling description popups.
     * @return {!Array.<{
     *   color: string,
     *   popupContent: string,
     *   xaxis: {from: number, to: number}
     * }>} A marks data structure for Flot to use.
     * @private
     */
    getEventMarks_: function() {
      var enabledEvents = [];
      var markings = [];
      var explanation;
      var date;

      for (var eventType in this.eventDetailsMap_) {
        if (this.eventDetailsMap_[eventType].enabled)
          enabledEvents.push(this.eventDetailsMap_[eventType]);
      }

      enabledEvents.forEach(function(eventValue) {
        eventValue.data.forEach(function(point) {
          if (point.time >= this.range_.start - timezoneOffset_ &&
              point.time <= this.range_.end - timezoneOffset_) {
            date = new Date(point.time + timezoneOffset_);
            explanation = '<b>' + eventValue.popupTitle + '<br/>' +
                date.toLocaleString() + '</b><br/>';

            for (var key in point) {
              if (key != 'time') {
                var datum = point[key];

                // We display all fields with a label-value pair.
                if ('label' in datum && 'value' in datum) {
                  explanation = explanation + '<b>' + datum.label + ': </b>' +
                      datum.value + ' <br/>';
                }
              }
            }
            markings.push({
              color: eventValue.color,
              popupContent: explanation,
              xaxis: { from: point.time, to: point.time }
            });
          } else {
            console.log('Event out of time range ' + this.range_.start +
                ' -> ' + this.range_.end + ' at: ' + point.time);
          }
        }, this);
      }, this);

      return markings;
    },

    /**
     * Return an object containing an array of series for Flot to chart, as well
     * as a series of axes (currently this will only be one axis).
     * @param {Array.<PerformanceMonitor.MetricDetails>} activeMetrics
     *     The metrics for which we are generating series.
     * @return {!{
     *   series: !Array.<{
     *     color: string,
     *     data: !Array<{time: number, value: number},
     *     yaxis: {min: number, max: number, labelWidth: number}
     *   },
     *   yaxes: !Array.<{min: number, max: number, labelWidth: number}>
     * }}
     * @private
     */
    getChartSeriesAndAxes_: function(activeMetrics) {
      var seriesList = [];
      var axisList = [];
      var axisMap = {};
      activeMetrics.forEach(function(metric) {
        var categoryId = metric.category.metricCategoryId;
        var yaxisNumber = axisMap[categoryId];

        // Add a new y-axis if we are encountering this category of metric
        // for the first time. Otherwise, update the existing y-axis with
        // a new max value if needed. (Presently, we expect only one category
        // of metric per chart, but this design permits more in the future.)
        if (yaxisNumber === undefined) {
          axisList.push({min: 0,
                         max: metric.maxValue * yAxisMargin_,
                         labelWidth: 60});
          axisMap[categoryId] = yaxisNumber = axisList.length;
        } else {
          axisList[yaxisNumber - 1].max =
              Math.max(axisList[yaxisNumber - 1].max,
                       metric.maxValue * yAxisMargin_);
        }

        // Create a Flot-style series for each data series in the metric.
        for (var i = 0; i < metric.data.length; ++i) {
          seriesList.push({
            color: metric.color,
            data: metric.data[i],
            label: i == 0 ? metric.name : null,
            yaxis: yaxisNumber
          });
        }
      }, this);

      return { series: seriesList, yaxes: axisList };
    },

    /**
     * Draw each chart which has at least one enabled metric, along with all
     * the event markers, if and only if we do not have outstanding calls for
     * data.
     */
    drawCharts: function() {
      // If we are currently waiting for data, do nothing - the callbacks will
      // re-call drawCharts when they are done. This way, we can avoid any
      // conflicts.
      if (this.fetchingData_())
        return;

      // All charts will share the same xaxis and events.
      var eventMarks = this.getEventMarks_();
      var xaxis = {
        mode: 'time',
        timeformat: this.range_.format,
        min: this.range_.start - timezoneOffset_,
        max: this.range_.end - timezoneOffset_
      };

      this.charts_.forEach(function(chart) {
        var activeMetrics = [];
        chart.metricIds.forEach(function(id) {
          if (this.metricDetailsMap_[id].enabled)
            activeMetrics.push(this.metricDetailsMap_[id]);
        }, this);

        if (!activeMetrics.length) {
          chart.hidden = true;
          return;
        }

        chart.mainDiv.hidden = false;

        var chartData = this.getChartSeriesAndAxes_(activeMetrics);

        // There is the possibility that we have no data for this particular
        // time window and metric, but Flot will not draw the grid without at
        // least one data point (regardless of whether that datapoint is
        // displayed). Thus, we will add the point (-1, -1) (which is guaranteed
        // not to show with our axis bounds), and force Flot to show the chart.
        if (chartData.series.length == 0)
          chartData.series = [[-1, -1]];

        chart.plot = $.plot(chart.grid, chartData.series, {
          yaxes: chartData.yaxes,
          xaxis: xaxis,
          points: { show: true, radius: 1},
          lines: { show: true},
          grid: {
            markings: eventMarks,
            hoverable: true,
            autoHighlight: true,
            backgroundColor: { colors: ['#fff', '#f0f6fc'] },
          },
        });

        // For each event in |eventMarks|, create also a label div, with left
        // edge colinear with the event vertical line. Top of label is
        // presently a hack-in, putting labels in three tiers of 25px height
        // each to avoid overlap. Will need something better.
        var labelTemplate = $('#label-template')[0];
        for (var i = 0; i < eventMarks.length; i++) {
          var mark = eventMarks[i];
          var point = chart.plot.pointOffset(
              {x: mark.xaxis.to, y: chartData.yaxes[0].max, yaxis: 1});
          var labelDiv = labelTemplate.cloneNode(true);
          labelDiv.innerHTML = mark.popupContent;
          labelDiv.style.left = point.left + 'px';
          labelDiv.style.top = (point.top + 100 * (i % 3)) + 'px';

          chart.grid.appendChild(labelDiv);
          labelDiv.hidden = true;
          chart.grid.hovers.push({x: mark.xaxis.to, div: labelDiv});
        }
      }, this);
    },
  };
  return {
    PerformanceMonitor: PerformanceMonitor
  };
});

var PerformanceMonitor = new performance_monitor.PerformanceMonitor();
