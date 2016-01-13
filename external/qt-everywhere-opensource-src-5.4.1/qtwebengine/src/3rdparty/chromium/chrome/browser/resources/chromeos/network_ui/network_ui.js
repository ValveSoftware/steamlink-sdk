// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NetworkUI = (function() {
  'use strict';

  // Properties to display in the network state table. Each entry can be either
  // a single state field or an array of state fields. If more than one is
  // specified then the first non empty value is used.
  var NETWORK_STATE_FIELDS = [
    'GUID',
    'Name',
    'Type',
    'ConnectionState',
    'connectable',
    'ErrorState',
    'WiFi.Security',
    ['Cellular.NetworkTechnology',
     'EAP.EAP'],
    'Cellular.ActivationState',
    'Cellular.RoamingState',
    'Cellular.OutOfCredits',
    'WiFi.SignalStrength'
  ];

  var FAVORITE_STATE_FIELDS = [
    'GUID',
    'Name',
    'Type',
    'profile_path',
    'visible',
    'onc_source'
  ];

  var LOG_LEVEL_CLASSNAME = {
    'Error': 'network-log-level-error',
    'User': 'network-log-level-user',
    'Event': 'network-log-level-event',
    'Debug': 'network-log-level-debug'
  };

  var LOG_LEVEL_CHECKBOX = {
    'Error': 'log-error',
    'User': 'log-user',
    'Event': 'log-event',
    'Debug': 'log-debug'
  };

  /**
   * Create a tag of log level.
   *
   * @param {string} level A string that represents log level.
   * @return {DOMElement} The created span element.
   */
  var createLevelTag = function(level) {
    var tag = document.createElement('span');
    tag.className = 'network-level-tag';
    tag.textContent = level;
    tag.classList.add(LOG_LEVEL_CLASSNAME[level]);
    return tag;
  };

  /**
   * Creates an element that contains the time, the event, the level and
   * the description of the given log entry.
   *
   * @param {Object} logEntry An object that represents a single line of log.
   * @return {DOMElement}  The created p element that represents the log entry.
   */
  var createLogEntryText = function(logEntry) {
    var level = logEntry['level'];
    if (!$(LOG_LEVEL_CHECKBOX[level]).checked)
      return null;
    var res = document.createElement('p');
    var textWrapper = document.createElement('span');
    var fileinfo = '';
    if ($('log-fileinfo').checked)
      fileinfo = logEntry['file'];
    var timestamp = '';
    if ($('log-timedetail').checked)
      timestamp = logEntry['timestamp'];
    else
      timestamp = logEntry['timestampshort'];
    textWrapper.textContent = loadTimeData.getStringF(
      'logEntryFormat',
      timestamp,
      fileinfo,
      logEntry['event'],
      logEntry['description']);
    res.appendChild(createLevelTag(level));
    res.appendChild(textWrapper);
    return res;
  };

  /**
   * Create event log entries.
   *
   * @param {Array.<string>} logEntries A array of strings that each string
   *     represents a log event in JSON format.
   */
  var createEventLog = function(logEntries) {
    var container = $('network-log-container');
    container.textContent = '';
    for (var i = 0; i < logEntries.length; ++i) {
      var entry = createLogEntryText(JSON.parse(logEntries[i]));
      if (entry)
        container.appendChild(entry);
    }
  };

  /**
   * Create a cell with a button for expanding a network state table row.
   *
   * @param {string} guid The GUID identifying the network.
   * @return {DOMElement} The created td element that displays the given value.
   */
  var createStateTableExpandButton = function(guid) {
    var cell = document.createElement('td');
    cell.className = 'state-table-expand-button-cell';
    var button = document.createElement('button');
    button.addEventListener('click', function(event) {
      toggleExpandRow(event.target, guid);
    });
    button.className = 'state-table-expand-button';
    button.textContent = '+';
    cell.appendChild(button);
    return cell;
  };

  /**
   * Create a cell in network state table.
   *
   * @param {string} value Content in the cell.
   * @return {DOMElement} The created td element that displays the given value.
   */
  var createStateTableCell = function(value) {
    var cell = document.createElement('td');
    cell.textContent = value || '';
    return cell;
  };

  /**
   * Create a row in the network state table.
   *
   * @param {Array} stateFields The state fields to use for the row.
   * @param {Object} state Property values for the network or favorite.
   * @return {DOMElement} The created tr element that contains the network
   *     state information.
   */
  var createStateTableRow = function(stateFields, state) {
    var row = document.createElement('tr');
    row.className = 'state-table-row';
    var guid = state.GUID;
    row.appendChild(createStateTableExpandButton(guid));
    for (var i = 0; i < stateFields.length; ++i) {
      var field = stateFields[i];
      var value = '';
      if (typeof field == 'string') {
        value = networkConfig.getValueFromProperties(state, field);
      } else {
        for (var j = 0; j < field.length; ++j) {
          value = networkConfig.getValueFromProperties(state, field[j]);
          if (value)
            break;
        }
      }
      if (field == 'GUID')
        value = value.slice(0, 8);
      row.appendChild(createStateTableCell(value));
    }
    return row;
  };

  /**
   * Create table for networks or favorites.
   *
   * @param {string} tablename The name of the table to be created.
   * @param {Array} stateFields The list of fields for the table.
   * @param {Array} states An array of network or favorite states.
   */
  var createStateTable = function(tablename, stateFields, states) {
    var table = $(tablename);
    var oldRows = table.querySelectorAll('.state-table-row');
    for (var i = 0; i < oldRows.length; ++i)
      table.removeChild(oldRows[i]);
    states.forEach(function(state) {
      table.appendChild(createStateTableRow(stateFields, state));
    });
  };

  /**
   * This callback function is triggered when the network log is received.
   *
   * @param {Object} data A JSON structure of event log entries.
   */
  var getNetworkLogCallback = function(data) {
    createEventLog(JSON.parse(data));
  };

  /**
   * This callback function is triggered when visible networks are received.
   *
   * @param {Array} data A list of network state information for each
   *     visible network.
   */
  var onVisibleNetworksReceived = function(states) {
    createStateTable('network-state-table', NETWORK_STATE_FIELDS, states);
  };

  /**
   * This callback function is triggered when favorite networks are received.
   *
   * @param {Object} data A list of network state information for each
   *     favorite network.
   */
  var onFavoriteNetworksReceived = function(states) {
    createStateTable('favorite-state-table', FAVORITE_STATE_FIELDS, states);
  };

  /**
   * Toggle the button state and add or remove a row displaying the complete
   * state information for a row.
   *
   * @param {DOMElement} btn The button that was clicked.
   * @param {string} guid GUID identifying the network.
   */
  var toggleExpandRow = function(btn, guid) {
    var cell = btn.parentNode;
    var row = cell.parentNode;
    if (btn.textContent == '-') {
      btn.textContent = '+';
      row.parentNode.removeChild(row.nextSibling);
    } else {
      btn.textContent = '-';
      var expandedRow = createExpandedRow(guid, row);
      row.parentNode.insertBefore(expandedRow, row.nextSibling);
    }
  };

  /**
   * Creates the expanded row for displaying the complete state as JSON.
   *
   * @param {Object} state Property values for the network or favorite.
   * @param {DOMElement} baseRow The unexpanded row associated with the new row.
   * @return {DOMElement} The created tr element for the expanded row.
   */
  var createExpandedRow = function(guid, baseRow) {
    var expandedRow = document.createElement('tr');
    expandedRow.className = 'state-table-row';
    var emptyCell = document.createElement('td');
    emptyCell.style.border = 'none';
    expandedRow.appendChild(emptyCell);
    var detailCell = document.createElement('td');
    detailCell.className = 'state-table-expanded-cell';
    detailCell.colSpan = baseRow.childNodes.length - 1;
    expandedRow.appendChild(detailCell);
    networkConfig.getProperties(guid, function(state) {
      if (networkConfig.lastError)
        detailCell.textContent = networkConfig.lastError;
      else
        detailCell.textContent = JSON.stringify(state, null, '\t');
    });
    return expandedRow;
  };

  /**
   * Requests a network log update.
   */
  var requestLog = function() {
    chrome.send('NetworkUI.getNetworkLog');
  };

  /**
   * Requests an update of all network info.
   */
  var requestNetworks = function() {
    networkConfig.getNetworks(
        { 'type': 'All', 'visible': true },
        onVisibleNetworksReceived);
    networkConfig.getNetworks(
        { 'type': 'All', 'configured': true },
        onFavoriteNetworksReceived);
  };

  /**
   * Sets refresh rate if the interval is found in the url.
   */
  var setRefresh = function() {
    var interval = parseQueryParams(window.location)['refresh'];
    if (interval && interval != '')
      setInterval(requestNetworks, parseInt(interval) * 1000);
  };

  /**
   * Get network information from WebUI.
   */
  document.addEventListener('DOMContentLoaded', function() {
    $('log-refresh').onclick = requestLog;
    $('log-error').checked = true;
    $('log-error').onclick = requestLog;
    $('log-user').checked = true;
    $('log-user').onclick = requestLog;
    $('log-event').checked = true;
    $('log-event').onclick = requestLog;
    $('log-debug').checked = false;
    $('log-debug').onclick = requestLog;
    $('log-fileinfo').checked = false;
    $('log-fileinfo').onclick = requestLog;
    $('log-timedetail').checked = false;
    $('log-timedetail').onclick = requestLog;
    setRefresh();
    requestLog();
    requestNetworks();
  });

  return {
    getNetworkLogCallback: getNetworkLogCallback
  };
})();
