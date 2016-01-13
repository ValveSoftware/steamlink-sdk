// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Counter accessor for Name Node. */
function getCounterNameFromCounterNode(node) {
  return node.childNodes[1];
}

/* Counter accessor for Value Node. */
function getCounterValueFromCounterNode(node) {
  return node.childNodes[3];
}

/* Counter accessor for Delta Node. */
function getCounterDeltaFromCounterNode(node) {
  return node.childNodes[5];
}

/* Timer accessor for Name Node. */
function getTimerNameFromTimerNode(node) {
  return node.childNodes[1];
}

/* Timer accessor for Value Node. */
function getTimerValueFromTimerNode(node) {
  return node.childNodes[3];
}

/* Timer accessor for Time Node. */
function getTimerTimeFromTimerNode(node) {
  return node.childNodes[5];
}

/* Timer accessor for Average Time Node. */
function getTimerAvgTimeFromTimerNode(node) {
  return node.childNodes[7];
}

/* Do the filter work.  Hide all nodes matching node.*/
function filterMatching(text, nodelist, functionToGetNameNode) {
  var showAll = text.length == 0;
  for (var i = 0, node; node = nodelist[i]; i++) {
    var name = functionToGetNameNode(node).innerHTML.toLowerCase();
    if (showAll || name.indexOf(text) >= 0)
      node.style.display = 'table-row';
    else
      node.style.display = 'none';
  }
}

/* Hides or shows counters based on the user's current filter selection. */
function doFilter() {
  var filter = $('filter');
  var text = filter.value.toLowerCase();
  var nodes = document.getElementsByName('counter');
  filterMatching(text, nodes, getCounterNameFromCounterNode);
  var nodes = document.getElementsByName('timer');
  filterMatching(text, nodes, getTimerNameFromTimerNode);
}

/* Colors the counters based on increasing or decreasing value. */
function doColor() {
  var nodes = document.getElementsByName('counter');
  for (var i = 0, node; node = nodes[i]; i++) {
    var child = getCounterDeltaFromCounterNode(node);
    var delta = child.innerHTML;
    if (delta > 0)
      child.style.color = 'Green';
    else if (delta == 0)
      child.style.color = 'Black';
    else
      child.style.color = 'Red';
  }
}

/* Counters with no values are null. Remove them. */
function removeNullValues() {
  var nodes = document.getElementsByName('counter');
  for (var i = nodes.length - 1; i >= 0; i--) {
    var node = nodes[i];
    var value = getCounterValueFromCounterNode(node).innerHTML;
    if (value == 'null')
      node.parentNode.removeChild(node);
  }
  var nodes = document.getElementsByName('timer');
  for (var i = 0, node; node = nodes[i]; i++) {
    var valueNode = getTimerValueFromTimerNode(node);
    if (valueNode.innerHTML == 'null')
      valueNode.innerHTML = '';
  }
}

/* Compute the average time for timers */
function computeTimes() {
  var nodes = document.getElementsByName('timer');
  for (var i = 0, node; node = nodes[i]; i++) {
    var count = getTimerValueFromTimerNode(node).innerHTML;
    if (count.length > 0) {
      var time = getTimerTimeFromTimerNode(node).innerHTML;
      var avg = getTimerAvgTimeFromTimerNode(node);
      avg.innerHTML = Math.round(time / count * 100) / 100;
    }
  }
}

/* All the work we do onload. */
function onLoadWork() {
  // This is the javascript code that processes the template:
  var input = new JsEvalContext(templateData);
  var output = $('t');
  jstProcess(input, output);

  // Add handlers to dynamically created HTML elements.
  var elements = document.getElementsByName('string-sort');
  for (var i = 0; i < elements.length; ++i)
    elements[i].onclick = function() { sortTable('string'); };

  elements = document.getElementsByName('number-sort');
  for (i = 0; i < elements.length; ++i)
    elements[i].onclick = function() { sortTable('number'); };

  doColor();
  removeNullValues();
  computeTimes();

  var filter = $('filter');
  filter.onkeyup = doFilter;
  filter.focus();
}

// The function should only be used as the event handler
// on a table cell element. To use it, put it in a <td> element:
//  <td onclick="sort('string')" ...>
//
// The function sorts rows after the row with onclick event handler.
//
// type: the data type, 'string', 'number'
function sortTable(type) {
  var cell = event.target;
  var cnum = cell.cellIndex;

  var row = cell.parentNode;
  var startIndex = row.rowIndex + 1;

  var tbody = row.parentNode;
  var table = tbody.parentNode;

  var rows = new Array();

  var indexes = new Array();
  // skip the first row
  for (var i = startIndex; i < table.rows.length; i++)
    rows.push(table.rows[i]);

  // a, b are strings
  function compareStrings(a, b) {
    if (a == b) return 0;
    if (a < b) return -1;
    return 1;
  }

  // a, b are numbers
  function compareNumbers(a, b) {
    var x = isNaN(a) ? 0 : a;
    var y = isNaN(b) ? 0 : b;
    return x - y;
  }

  var sortFunc;
  if (type === 'string') {
    sortFunc = function(a, b) {
      var x = a.cells[cnum].innerText;
      var y = b.cells[cnum].innerText;
      return compareStrings(x, y);
    };

  } else if (type === 'number') {
    sortFunc = function(a, b) {
      var x = parseFloat(a.cells[cnum].innerText);
      var y = parseFloat(b.cells[cnum].innerText);
      return compareNumbers(x, y);
    };
  }

  rows.sort(sortFunc);

  // change tables
  if (cell._reverse) {
    for (var i = rows.length - 1; i >= 0; i--)
      tbody.appendChild(rows[i]);
    cell._reverse = false;
  } else {
    for (var i = 0; i < rows.length; i++)
      tbody.appendChild(rows[i]);
    cell._reverse = true;
  }
}

document.addEventListener('DOMContentLoaded', onLoadWork);
