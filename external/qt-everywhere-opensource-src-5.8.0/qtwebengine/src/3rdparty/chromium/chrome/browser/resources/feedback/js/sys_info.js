// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The global load time data that contains the localized strings that we will
 * get from the main page when this page first loads.
 */
var loadTimeData = null;

function getValueDivForButton(button) {
  return $(button.id.substr(0, button.id.length - 4));
}

function getButtonForValueDiv(valueDiv) {
  return $(valueDiv.id + '-btn');
}

/**
 * Toggles whether an item is collapsed or expanded.
 */
function changeCollapsedStatus() {
  var valueDiv = getValueDivForButton(this);
  if (valueDiv.parentNode.className == 'number-collapsed') {
    valueDiv.parentNode.className = 'number-expanded';
    this.textContent = loadTimeData.getString('sysinfoPageCollapseBtn');
  } else {
    valueDiv.parentNode.className = 'number-collapsed';
    this.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
  }
}

/**
 * Collapses all log items.
 */
function collapseAll() {
  var valueDivs = document.getElementsByClassName('stat-value');
  for (var i = 0; i < valueDivs.length; i++) {
    var button = getButtonForValueDiv(valueDivs[i]);
    if (button && button.className != 'button-hidden') {
      button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
      valueDivs[i].parentNode.className = 'number-collapsed';
    }
  }
}

/**
 * Expands all log items.
 */
function expandAll() {
  var valueDivs = document.getElementsByClassName('stat-value');
  for (var i = 0; i < valueDivs.length; i++) {
    var button = getButtonForValueDiv(valueDivs[i]);
    if (button && button.className != 'button-hidden') {
      button.textContent = loadTimeData.getString('sysinfoPageCollapseBtn');
      valueDivs[i].parentNode.className = 'number-expanded';
    }
  }
}

/**
 * Collapse only those log items with multi-line values.
 */
function collapseMultiLineStrings() {
  var valueDivs = document.getElementsByClassName('stat-value');
  var nameDivs = document.getElementsByClassName('stat-name');
  for (var i = 0; i < valueDivs.length; i++) {
    var button = getButtonForValueDiv(valueDivs[i]);
    button.onclick = changeCollapsedStatus;
    if (valueDivs[i].scrollHeight > (nameDivs[i].scrollHeight * 2)) {
      button.className = '';
      button.textContent = loadTimeData.getString('sysinfoPageExpandBtn');
      valueDivs[i].parentNode.className = 'number-collapsed';
    } else {
      button.className = 'button-hidden';
      valueDivs[i].parentNode.className = 'number';
    }
  }
}

function createNameCell(key) {
  var nameCell = document.createElement('td');
  nameCell.setAttribute('class', 'name');
  var nameDiv = document.createElement('div');
  nameDiv.setAttribute('class', 'stat-name');
  nameDiv.appendChild(document.createTextNode(key));
  nameCell.appendChild(nameDiv);
  return nameCell;
}

function createButtonCell(key) {
  var buttonCell = document.createElement('td');
  buttonCell.setAttribute('class', 'button-cell');
  var button = document.createElement('button');
  button.setAttribute('id', '' + key + '-value-btn');
  buttonCell.appendChild(button);
  return buttonCell;
}

function createValueCell(key, value) {
  var valueCell = document.createElement('td');
  var valueDiv = document.createElement('div');
  valueDiv.setAttribute('class', 'stat-value');
  valueDiv.setAttribute('id', '' + key + '-value');
  valueDiv.appendChild(document.createTextNode(value));
  valueCell.appendChild(valueDiv);
  return valueCell;
}

function createTableRow(key, value) {
  var row = document.createElement('tr');
  row.appendChild(createNameCell(key));
  row.appendChild(createButtonCell(key));
  row.appendChild(createValueCell(key, value));
  return row;
}

/**
 * Builds the system information table row by row.
 * @param {Element} table The DOM table element to which the newly created rows
 * will be appended.
 * @param {Object} systemInfo The system information that will be used to fill
 * the table.
 */
function createTable(table, systemInfo) {
  for (var key in systemInfo) {
    var item = systemInfo[key];
    table.appendChild(createTableRow(item['key'], item['value']));
  }
}

/**
 * The callback which will be invoked by the parent window, when the system
 * information is ready.
 * @param {Object} systemInfo The system information that will be used to fill
 * the table.
 */
function onSysInfoReady(systemInfo) {
  createTable($('detailsTable'), systemInfo);

  $('collapseAllBtn').onclick = collapseAll;
  $('expandAllBtn').onclick = expandAll;

  collapseMultiLineStrings();

  $('status').textContent = '';
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  loadTimeData = getLoadTimeData();
  i18nTemplate.process(document, loadTimeData);
  getFullSystemInfo(onSysInfoReady);
};
