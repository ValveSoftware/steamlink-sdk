// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Debug information about an active copresence directive.
 * @typedef {{
 *   type: string,
 *   medium: string,
 *   duration: string
 * }}
 */
var Directive;

/**
 * Debug information about a recent copresence token.
 * @typedef {{
 *   id: string,
 *   statuses: string,
 *   medium: string,
 *   time: string
 * }}
 */
var Token;

/**
 * Callback to refresh the list of directives.
 * @param {Array<Directive>} directives
 */
function refreshDirectives(directives) {
  var table = $('directives-table').tBodies[0];

  // Fix the directives table to have the correct number of rows.
  while (table.rows.length < directives.length)
    table.insertRow();
  while (table.rows.length > directives.length)
    table.deleteRow();

  // Populate the directives into the table.
  directives.forEach(function(directive, index) {
    var row = table.rows.item(index);
    while (row.cells.length < 3)
      row.insertCell();

    row.cells.item(0).textContent = directive.type;
    row.cells.item(1).textContent = directive.medium;
    row.cells.item(2).textContent = directive.duration;
  });
}

/**
 * Callback to add or update transmitted tokens.
 * @param {Token} token
 */
function updateTransmittedToken(token) {
  updateTokenTable($('transmitted-tokens-table'), token);
}

/**
 * Callback to add or update received tokens.
 * @param {Token} token
 */
function updateReceivedToken(token) {
  updateTokenTable($('received-tokens-table'), token);
}

/**
 * Callback to clear out the token tables.
 */
function clearTokens() {
  clearTable($('transmitted-tokens-table'));
  clearTable($('received-tokens-table'));
}

/**
 * Add or update a token in the specified table.
 * @param {HTMLTableElement} table
 * @param {Token} token
 */
function updateTokenTable(table, token) {
  var rows = table.tBodies[0].rows;

  var index;
  for (index = 0; index < rows.length; index++) {
    var row = rows.item(index);
    if (row.cells[0].textContent == token.id) {
      updateTokenRow(row, token);
      break;
    }
  }

  if (index == rows.length)
    updateTokenRow(table.tBodies[0].insertRow(), token);
}

/**
 * Update a token on the specified row.
 * @param {HTMLTableRowElement} row
 * @param {Token} token
 */
function updateTokenRow(row, token) {
  while (row.cells.length < 4)
    row.insertCell();
  row.className = token.statuses;

  row.cells[0].textContent = token.id;
  row.cells[1].textContent =
      token.statuses.replace('confirmed', '(Confirmed)');
  row.cells[2].textContent = token.medium;
  row.cells[3].textContent = token.time;
}

/**
 * Delete all the rows in a table.
 * @param {HTMLTableElement} table
 */
function clearTable(table) {
  var body = table.tBodies[0];
  while (body.rows.length > 0)
    body.deleteRow();
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('populateCopresenceState');

  $('reset-button').addEventListener('click', function() {
    if (confirm(loadTimeData.getString('confirm_delete')))
      chrome.send('clearCopresenceState');
  });
});
