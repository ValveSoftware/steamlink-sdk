// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays the log messages from various resources in an
 * interactive log visualizer
 *
 *   - Filter checkboxes
 *   - Filter text inputs
 *   - Display the log by different sections: time|level|process|description
 *
 */
var CrosLogVisualizerView = (function() {
  'use strict';

  // Inherits from DivView.
  var superClass = DivView;

  // Special classes (defined in log_visualizer_view.css)
  var LOG_CONTAINER_CLASSNAME = 'cros-log-visualizer-container';
  var LOG_FILTER_PNAME_BLOCK_CLASSNAME =
      'cros-log-visualizer-filter-pname-block';
  var LOG_CELL_HEADER_CLASSNAME = 'cros-log-visualizer-td-head';
  var LOG_CELL_TIME_CLASSNAME = 'cros-log-visualizer-td-time';
  var LOG_CELL_PNAME_CLASSNAME = 'cros-log-visualizer-td-pname';
  var LOG_CELL_PID_CLASSNAME = 'cros-log-visualizer-td-pid';
  var LOG_CELL_DESCRIPTION_CLASSNAME = 'cros-log-visualizer-td-description';
  var LOG_CELL_LEVEL_CLASSNAME = 'cros-log-visualizer-td-level';
  var LOG_CELL_LEVEL_CLASSNAME_LIST = {
    'Error': 'cros-log-visualizer-td-level-error',
    'Warning': 'cros-log-visualizer-td-level-warning',
    'Info': 'cros-log-visualizer-td-level-info',
    'Unknown': 'cros-log-visualizer-td-level-unknown'
  };

  /**
   * @constructor
   */
  function CrosLogVisualizerView() {
    assertFirstConstructorCall(CrosLogVisualizerView);

    // Call superclass's constructor.
    superClass.call(this, CrosLogVisualizerView.MAIN_BOX_ID);

    // Stores log entry objects
    this.logEntries = [];
    // Stores current search query
    this.currentQuery = '';
    // Stores raw text data of log
    this.logData = '';
    // Stores all the unique process names
    this.pNames = [];
    // References to special HTML elements in log_visualizer_view.html
    this.pNameCheckboxes = {};
    this.levelCheckboxes = {};
    this.tableEntries = [];

    this.initialize();
  }

  CrosLogVisualizerView.TAB_ID = 'tab-handle-cros-log-visualizer';
  CrosLogVisualizerView.TAB_NAME = 'Log Visualizer';
  CrosLogVisualizerView.TAB_HASH = '#visualizer';

  // IDs for special HTML elements in log_visualizer_view.html
  CrosLogVisualizerView.MAIN_BOX_ID = 'cros-log-visualizer-tab-content';
  CrosLogVisualizerView.LOG_TABLE_ID = 'cros-log-visualizer-log-table';
  CrosLogVisualizerView.LOG_FILTER_PNAME_ID =
      'cros-log-visualizer-filter-pname';
  CrosLogVisualizerView.LOG_SEARCH_INPUT_ID =
      'cros-log-visualizer-search-input';
  CrosLogVisualizerView.LOG_SEARCH_SAVE_BTN_ID = 'cros-log-visualizer-save-btn';
  CrosLogVisualizerView.LOG_VISUALIZER_CONTAINER_ID =
      'cros-log-visualizer-visualizer-container';

  cr.addSingletonGetter(CrosLogVisualizerView);

  /**
   * Contains types of logs we are interested in
   */
  var LOGS_LIST = {
    'NETWORK_LOG': 1,
    'SYSTEM_LOG': 2
  };

  /**
   * Contains headers of the log table
   */
  var TABLE_HEADERS_LIST = ['Level', 'Time', 'Process', 'PID', 'Description'];

  CrosLogVisualizerView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called during the initialization of the View. Adds the system log
     * listener into Browser_Bridge so that the system log can be retrieved.
     */
    initialize: function() {
      g_browser.addSystemLogObserver(this);
      $(CrosLogVisualizerView.LOG_SEARCH_INPUT_ID).addEventListener('keyup',
          this.onSearchQueryChange_.bind(this));
      $(CrosLogVisualizerView.LOG_SEARCH_SAVE_BTN_ID).addEventListener(
          'click', this.onSaveBtnClicked_.bind(this));
    },

    /**
     * Called when the save button is clicked. Saves the current filter query
     * to the mark history. And highlights the matched text with colors.
     */
    onSaveBtnClicked_: function() {
      this.marker.addMarkHistory(this.currentQuery);
      // Clears the filter query
      $(CrosLogVisualizerView.LOG_SEARCH_INPUT_ID).value = '';
      this.currentQuery = '';
      // Refresh the table
      this.populateTable();
      this.filterLog();
    },

    onSearchQueryChange_: function() {
      var inputField = $(CrosLogVisualizerView.LOG_SEARCH_INPUT_ID);
      this.currentQuery = inputField.value;
      this.filterLog();
    },

    /**
     * Creates the log table where each row represents a entry of log.
     * This function is called if and only if the log is received from system
     * level.
     */
    populateTable: function() {
      var logTable = $(CrosLogVisualizerView.LOG_TABLE_ID);
      logTable.innerHTML = '';
      this.tableEntries.length = 0;
      // Create entries
      for (var i = 0; i < this.logEntries.length; i++) {
        this.logEntries[i].rowNum = i;
        var row = this.createTableRow(this.logEntries[i]);
        logTable.appendChild(row);
      }
    },

    /**
     * Creates the single row of the table where each row is a representation
     * of the logEntry object.
     */
    createTableRow: function(entry) {
      var row = document.createElement('tr');
      for (var i = 0; i < 5; i++) {
        // Creates rows
        addNode(row, 'td');
      }
      var cells = row.childNodes;
      // Level cell
      cells[0].className = LOG_CELL_LEVEL_CLASSNAME;
      var levelTag = addNodeWithText(cells[0], 'p', entry.level);
      levelTag.className = LOG_CELL_LEVEL_CLASSNAME_LIST[entry.level];

      // Time cell
      cells[1].className = LOG_CELL_TIME_CLASSNAME;
      cells[1].textContent = entry.getTime();

      // Process name cell
      cells[2].className = LOG_CELL_PNAME_CLASSNAME;
      this.marker.getHighlightedEntry(entry, 'processName', cells[2]);

      // Process ID cell
      cells[3].className = LOG_CELL_PID_CLASSNAME;
      this.marker.getHighlightedEntry(entry, 'processID', cells[3]);

      // Description cell
      cells[4].className = LOG_CELL_DESCRIPTION_CLASSNAME;
      this.marker.getHighlightedEntry(entry, 'description', cells[4]);

      // Add the row into this.tableEntries for future reference
      this.tableEntries.push(row);
      return row;
    },

    /**
     * Regenerates the table and filter.
     */
    refresh: function() {
      this.createFilter();
      this.createLogMaker();
      this.populateTable();
      this.createVisualizer();
    },

    /**
     * Uses the search query to match the pattern in different fields of entry.
     */
    patternMatch: function(entry, pattern) {
      return entry.processID.match(pattern) ||
             entry.processName.match(pattern) ||
             entry.level.match(pattern) ||
             entry.description.match(pattern);
    },

    /**
     * Filters the log to show/hide the rows in the table.
     * Each logEntry instance has a visibility property. This function
     * shows or hides the row only based on this property.
     */
    filterLog: function() {
      // Supports regular expression
      var pattern = new RegExp(this.currentQuery, 'i');
      for (var i = 0; i < this.logEntries.length; i++) {
        var entry = this.logEntries[i];
        // Filters the result by pname and level
        var pNameCheckbox = this.pNameCheckboxes[entry.processName];
        var levelCheckbox = this.levelCheckboxes[entry.level];
        entry.visibility = pNameCheckbox.checked && levelCheckbox.checked &&
            !this.visualizer.isOutOfBound(entry);
        if (this.currentQuery) {
          // If the search query is not empty, filter the result by query
          entry.visibility = entry.visibility &&
              this.patternMatch(entry, pattern);
        }
        // Changes style of HTML row based on the visibility of logEntry
        if (entry.visibility) {
          this.tableEntries[i].style.display = 'table-row';
        } else {
          this.tableEntries[i].style.display = 'none';
        }
      }
      this.filterVisualizer();
    },

    /**
     * Initializes filter tags and checkboxes. There are two types of filters:
     * Level and Process. Level filters are static that we have only 4 levels
     * in total but process filters are dynamically changing based on the log.
     * The filter layout looks like:
     *  |-----------------------------------------------------------------|
     *  |                                                                 |
     *  |                     Section of process filter                   |
     *  |                                                                 |
     *  |-----------------------------------------------------------------|
     *  |                                                                 |
     *  |                      Section of level filter                    |
     *  |                                                                 |
     *  |-----------------------------------------------------------------|
     */
    createFilter: function() {
      this.createFilterByPName();
      this.levelCheckboxes = {
        'Error': $('checkbox-error'),
        'Warning': $('checkbox-warning'),
        'Info': $('checkbox-info'),
        'Unknown': $('checkbox-unknown')
      };

      for (var level in this.levelCheckboxes) {
        this.levelCheckboxes[level].addEventListener(
            'change', this.onFilterChange_.bind(this));
      }
    },

    /**
     * Helper function of createFilter(). Create filter section of
     * process filters.
     */
    createFilterByPName: function() {
      var filterContainerDiv = $(CrosLogVisualizerView.LOG_FILTER_PNAME_ID);
      filterContainerDiv.innerHTML = 'Process: ';
      for (var i = 0; i < this.pNames.length; i++) {
        var pNameBlock = this.createPNameBlock(this.pNames[i]);
        filterContainerDiv.appendChild(pNameBlock);
      }
    },

    /**
     * Helper function of createFilterByPName(). Create a single filter block in
     * the section of process filters.
     */
    createPNameBlock: function(pName) {
      var block = document.createElement('span');
      block.className = LOG_FILTER_PNAME_BLOCK_CLASSNAME;

      var tag = document.createElement('label');
      var span = document.createElement('span');
      span.textContent = pName;

      var checkbox = document.createElement('input');
      checkbox.type = 'checkbox';
      checkbox.name = pName;
      checkbox.value = pName;
      checkbox.checked = true;
      checkbox.addEventListener('change', this.onFilterChange_.bind(this));
      this.pNameCheckboxes[pName] = checkbox;

      tag.appendChild(checkbox);
      tag.appendChild(span);
      block.appendChild(tag);

      return block;
    },

    /**
     * Click handler for filter checkboxes. Everytime a checkbox is clicked,
     * the visibility of related logEntries are changed.
     */
    onFilterChange_: function() {
      this.filterLog();
    },

    /**
     * Creates a visualizer that visualizes the logs as a timeline graph
     * during the initialization of the View.
     */
    createVisualizer: function() {
      this.visualizer = new CrosLogVisualizer(this,
          CrosLogVisualizerView.LOG_VISUALIZER_CONTAINER_ID);
      this.visualizer.updateEvents(this.logEntries);
    },

    /**
     * Sync the visibility of log entries with the visualizer.
     */
    filterVisualizer: function() {
      this.visualizer.updateEvents(this.logEntries);
    },

    /**
     * Called during the initialization. It creates the log marker that
     * highlights log text.
     */
    createLogMaker: function() {
      this.marker = new CrosLogMarker(this);
    },

    /**
     * Given a row text line of log, a logEntry instance is initialized and used
     * for parsing. After the text is parsed, we put the instance into
     * logEntries which is an array for storing. This function is called when
     * the data is received from Browser Bridge.
     */
    addLogEntry: function(logType, textEntry) {
      var newEntry = new CrosLogEntry();
      if (logType == LOGS_LIST.NETWORK_LOG) {
        newEntry.tokenizeNetworkLog(textEntry);
      } else {
        //TODO(shinfan): Add more if cases here
      }
      this.logEntries.push(newEntry);

      // Record pname
      var pName = newEntry.processName;
      if (this.pNames.indexOf(pName) == -1) {
        this.pNames.push(pName);
      }
    },

    /*
     * Asynchronous call back function from Browser Bridge.
     */
    onSystemLogChanged: function(callback) {
      if (callback.log == this.logData) return;
      this.logData = callback.log;
      // Clear the old array by setting length to zero
      this.logEntries.length = 0;
      var entries = callback.log.split('\n');
      for (var i = 1; i < entries.length; i++) {
        this.addLogEntry(LOGS_LIST.NETWORK_LOG, entries[i]);
      }
      this.refresh();
    }
  };

  return CrosLogVisualizerView;
})();
