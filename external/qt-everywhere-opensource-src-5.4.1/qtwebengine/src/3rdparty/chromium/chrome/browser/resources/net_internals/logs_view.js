// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays network related log data and is specific fo ChromeOS.
 * We get log data from chrome by filtering system logs for network related
 * keywords. Logs are not fetched until we actually need them.
 */
var LogsView = (function() {
  'use strict';

  // Special classes (defined in logs_view.css).
  var LOG_ROW_COLLAPSED_CLASSNAME = 'logs-view-log-row-collapsed';
  var LOG_ROW_EXPANDED_CLASSNAME = 'logs-view-log-row-expanded';
  var LOG_CELL_TEXT_CLASSNAME = 'logs-view-log-cell-text';
  var LOG_CELL_LOG_CLASSNAME = 'logs-view-log-cell-log';
  var LOG_TABLE_BUTTON_COLUMN_CLASSNAME = 'logs-view-log-table-button-column';
  var LOG_BUTTON_CLASSNAME = 'logs-view-log-button';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function LogsView() {
    assertFirstConstructorCall(LogsView);

    // Call superclass's constructor.
    superClass.call(this, LogsView.MAIN_BOX_ID);

    var tableDiv = $(LogsView.TABLE_ID);
    this.rows = [];
    this.populateTable(tableDiv, LOG_FILTER_LIST);
    $(LogsView.GLOBAL_SHOW_BUTTON_ID).addEventListener('click',
        this.onGlobalChangeVisibleClick_.bind(this, true));
    $(LogsView.GLOBAL_HIDE_BUTTON_ID).addEventListener('click',
        this.onGlobalChangeVisibleClick_.bind(this, false));
    $(LogsView.REFRESH_LOGS_BUTTON_ID).addEventListener('click',
        this.onLogsRefresh_.bind(this));
  }

  LogsView.TAB_ID = 'tab-handle-logs';
  LogsView.TAB_NAME = 'Logs';
  LogsView.TAB_HASH = '#logs';

  // IDs for special HTML elements in logs_view.html
  LogsView.MAIN_BOX_ID = 'logs-view-tab-content';
  LogsView.TABLE_ID = 'logs-view-log-table';
  LogsView.GLOBAL_SHOW_BUTTON_ID = 'logs-view-global-show-btn';
  LogsView.GLOBAL_HIDE_BUTTON_ID = 'logs-view-global-hide-btn';
  LogsView.REFRESH_LOGS_BUTTON_ID = 'logs-view-refresh-btn';

  cr.addSingletonGetter(LogsView);

  /**
   * Contains log keys we are interested in.
   */
  var LOG_FILTER_LIST = [
    {
      key: 'syslog',
    },
    {
      key: 'ui_log',
    },
    {
      key: 'chrome_system_log',
    },
    {
      key: 'chrome_log',
    }
  ];

  LogsView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called during View's initialization. Creates the row of a table logs will
     * be shown in. Each row has 4 cells.
     *
     * First cell's content will be set to |logKey|, second will contain a
     * button that will be used to show or hide third cell, which will contain
     * the filtered log.
     * |logKey| also tells us which log we are getting data from.
     */
    createTableRow: function(logKey) {
      var row = document.createElement('tr');

      var cells = [];
      for (var i = 0; i < 3; i++) {
        var rowCell = document.createElement('td');
        cells.push(rowCell);
        row.appendChild(rowCell);
      }
      // Log key cell.
      cells[0].className = LOG_CELL_TEXT_CLASSNAME;
      cells[0].textContent = logKey;
      // Cell log is displayed in. Log content is in div element that is
      // initially hidden and empty.
      cells[2].className = LOG_CELL_TEXT_CLASSNAME;
      var logDiv = document.createElement('div');
      logDiv.textContent = '';
      logDiv.className = LOG_CELL_LOG_CLASSNAME;
      logDiv.id = 'logs-view.log-cell.' + this.rows.length;
      cells[2].appendChild(logDiv);

      // Button that we use to show or hide div element with log content. Logs
      // are not visible initially, so we initialize button accordingly.
      var expandButton = document.createElement('button');
      expandButton.textContent = 'Show...';
      expandButton.className = LOG_BUTTON_CLASSNAME;
      expandButton.addEventListener('click',
                                    this.onButtonClicked_.bind(this, row));

      // Cell that contains show/hide button.
      cells[1].appendChild(expandButton);
      cells[1].className = LOG_TABLE_BUTTON_COLUMN_CLASSNAME;

      // Initially, log is not visible.
      row.className = LOG_ROW_COLLAPSED_CLASSNAME;

      // We will need those to process row buttons' onclick events.
      row.logKey = logKey;
      row.expandButton = expandButton;
      row.logDiv = logDiv;
      row.logVisible = false;
      this.rows.push(row);

      return row;
    },

    /**
     * Initializes |tableDiv| to represent data from |logList| which should be
     * of type LOG_FILTER_LIST.
     */
    populateTable: function(tableDiv, logList) {
      for (var i = 0; i < logList.length; i++) {
        var logSource = this.createTableRow(logList[i].key);
        tableDiv.appendChild(logSource);
      }
    },

    /**
     * Processes clicks on buttons that show or hide log contents in log row.
     * Row containing the clicked button is given to the method since it
     * contains all data we need to process the click (unlike button object
     * itself).
     */
    onButtonClicked_: function(containingRow) {
      if (!containingRow.logVisible) {
        containingRow.className = LOG_ROW_EXPANDED_CLASSNAME;
        containingRow.expandButton.textContent = 'Hide...';
        var logDiv = containingRow.logDiv;
        if (logDiv.textContent == '') {
          logDiv.textContent = 'Getting logs...';
          // Callback will be executed by g_browser.
          g_browser.getSystemLog(containingRow.logKey,
                                 containingRow.logDiv.id);
        }
      } else {
        containingRow.className = LOG_ROW_COLLAPSED_CLASSNAME;
        containingRow.expandButton.textContent = 'Show...';
      }
      containingRow.logVisible = !containingRow.logVisible;
    },

    /**
     * Processes click on one of the buttons that are used to show or hide all
     * logs we care about.
     */
    onGlobalChangeVisibleClick_: function(isShowAll) {
      for (var row in this.rows) {
        if (isShowAll != this.rows[row].logVisible) {
          this.onButtonClicked_(this.rows[row]);
        }
      }
    },

    /**
     * Processes click event on the button we use to refresh fetched logs. We
     * get the newest logs from libcros, and refresh content of the visible log
     * cells.
     */
    onLogsRefresh_: function() {
      g_browser.refreshSystemLogs();

      var visibleLogRows = [];
      var hiddenLogRows = [];
      for (var row in this.rows) {
        if (this.rows[row].logVisible) {
          visibleLogRows.push(this.rows[row]);
        } else {
          hiddenLogRows.push(this.rows[row]);
        }
      }

      // We have to refresh text content in visible rows.
      for (row in visibleLogRows) {
        visibleLogRows[row].logDiv.textContent = 'Getting logs...';
        g_browser.getSystemLog(visibleLogRows[row].logKey,
                               visibleLogRows[row].logDiv.id);
      }

      // In hidden rows we just clear potential log text, so we know we have to
      // get new contents when we show the row next time.
      for (row in hiddenLogRows) {
        hiddenLogRows[row].logDiv.textContent = '';
      }
    }
  };

  return LogsView;
})();
