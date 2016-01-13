// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('accessibility', function() {
  'use strict';

  // Keep in sync with view_message_enums.h
  var AccessibilityModeFlag = {
    Platform: 1 << 0,
    FullTree: 1 << 1
  }

  var AccessibilityMode = {
    Off: 0,
    Complete:
        AccessibilityModeFlag.Platform | AccessibilityModeFlag.FullTree,
    EditableTextOnly: AccessibilityModeFlag.Platform,
    TreeOnly: AccessibilityModeFlag.FullTree
  }

  function isAccessibilityComplete(mode) {
    return ((mode & AccessibilityMode.Complete) == AccessibilityMode.Complete);
  }

  function requestData() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'targets-data.json', false);
    xhr.send(null);
    if (xhr.status === 200) {
      console.log(xhr.responseText);
      return JSON.parse(xhr.responseText);
    }
    return [];
  }

  // TODO(aboxhall): add a mechanism to request individual and global a11y
  // mode, xhr them on toggle... or just re-requestData and be smarter about
  // ID-ing rows?

  function toggleAccessibility(data, element) {
    chrome.send('toggleAccessibility',
                [String(data.processId), String(data.routeId)]);
    var a11y_was_on = (element.textContent.match(/on/) != null);
    element.textContent = ' accessibility ' + (a11y_was_on ? ' off' : ' on');
    var row = element.parentElement;
    if (a11y_was_on) {
      while (row.lastChild != element)
        row.removeChild(row.lastChild);
    } else {
      row.appendChild(document.createTextNode(' | '));
      row.appendChild(createShowAccessibilityTreeElement(data, row, false));
    }
  }

  function requestAccessibilityTree(data, element) {
    chrome.send('requestAccessibilityTree',
                [String(data.processId), String(data.routeId)]);
  }

  function toggleGlobalAccessibility() {
    chrome.send('toggleGlobalAccessibility');
    document.location.reload(); // FIXME see TODO above
  }

  function initialize() {
    console.log('initialize');
    var data = requestData();

    addGlobalAccessibilityModeToggle(data['global_a11y_mode']);

    $('pages').textContent = '';

    var list = data['list'];
    for (var i = 0; i < list.length; i++) {
      addToPagesList(list[i]);
    }
  }

  function addGlobalAccessibilityModeToggle(global_a11y_mode) {
    var full_a11y_on = isAccessibilityComplete(global_a11y_mode);
    $('toggle_global').textContent = (full_a11y_on ? 'on' : 'off');
    $('toggle_global').addEventListener('click',
                                        toggleGlobalAccessibility);
  }

  function addToPagesList(data) {
    // TODO: iterate through data and pages rows instead
    var id = data['processId'] + '.' + data['routeId'];
    var row = document.createElement('div');
    row.className = 'row';
    row.id = id;
    formatRow(row, data);

    row.processId = data.processId;
    row.routeId = data.routeId;

    var list = $('pages');
    list.appendChild(row);
  }

  function formatRow(row, data) {
    if (!('url' in data)) {
      if ('error' in data) {
        row.appendChild(createErrorMessageElement(data, row));
        return;
      }
    }
    var properties = ['favicon_url', 'name', 'url'];
    for (var j = 0; j < properties.length; j++)
      row.appendChild(formatValue(data, properties[j]));

    row.appendChild(createToggleAccessibilityElement(data));
    if (isAccessibilityComplete(data['a11y_mode'])) {
      row.appendChild(document.createTextNode(' | '));
      if ('tree' in data) {
        row.appendChild(createShowAccessibilityTreeElement(data, row, true));
        row.appendChild(document.createTextNode(' | '));
        row.appendChild(createHideAccessibilityTreeElement(row.id));
        row.appendChild(createAccessibilityTreeElement(data));
      }
      else {
        row.appendChild(createShowAccessibilityTreeElement(data, row, false));
        if ('error' in data)
          row.appendChild(createErrorMessageElement(data, row));
      }
    }
  }

  function formatValue(data, property) {
    var value = data[property];

    if (property == 'favicon_url') {
      var faviconElement = document.createElement('img');
      if (value)
        faviconElement.src = value;
      faviconElement.alt = "";
      return faviconElement;
    }

    var text = value ? String(value) : '';
    if (text.length > 100)
      text = text.substring(0, 100) + '\u2026';  // ellipsis

    var span = document.createElement('span');
    span.textContent = ' ' + text + ' ';
    span.className = property;
    return span;
  }

  function createToggleAccessibilityElement(data) {
    var link = document.createElement('a');
    link.setAttribute('href', '#');
    var full_a11y_on = isAccessibilityComplete(data['a11y_mode']);
    link.textContent = 'accessibility ' + (full_a11y_on ? 'on' : 'off');
    link.addEventListener('click',
                          toggleAccessibility.bind(this, data, link));
    return link;
  }

  function createShowAccessibilityTreeElement(data, row, opt_refresh) {
    var link = document.createElement('a');
    link.setAttribute('href', '#');
    if (opt_refresh)
      link.textContent = 'refresh accessibility tree';
    else
      link.textContent = 'show accessibility tree';
    link.id = row.id + ':showTree';
    link.addEventListener('click',
                          requestAccessibilityTree.bind(this, data, link));
    return link;
  }

  function createHideAccessibilityTreeElement(id) {
    var link = document.createElement('a');
    link.setAttribute('href', '#');
    link.textContent = 'hide accessibility tree';
    link.addEventListener('click',
                          function() {
        $(id + ':showTree').textContent = 'show accessibility tree';
        var existingTreeElements = $(id).getElementsByTagName('pre');
        for (var i = 0; i < existingTreeElements.length; i++)
          $(id).removeChild(existingTreeElements[i]);
        var row = $(id);
        while (row.lastChild != $(id + ':showTree'))
          row.removeChild(row.lastChild);
    });
    return link;
  }

  function createErrorMessageElement(data) {
    var errorMessageElement = document.createElement('div');
    var errorMessage = data.error;
    errorMessageElement.innerHTML = errorMessage + '&nbsp;';
    var closeLink = document.createElement('a');
    closeLink.href='#';
    closeLink.textContent = '[close]';
    closeLink.addEventListener('click', function() {
        var parentElement = errorMessageElement.parentElement;
        parentElement.removeChild(errorMessageElement);
        if (parentElement.childElementCount == 0)
          parentElement.parentElement.removeChild(parentElement);
    });
    errorMessageElement.appendChild(closeLink);
    return errorMessageElement;
  }

  function showTree(data) {
    var id = data.processId + '.' + data.routeId;
    var row = $(id);
    if (!row)
      return;

    row.textContent = '';
    formatRow(row, data);
  }

  function createAccessibilityTreeElement(data) {
    var treeElement = document.createElement('pre');
    var tree = data.tree;
    treeElement.textContent = tree;
    return treeElement;
  }

  return {
    initialize: initialize,
    showTree: showTree
  };
});

document.addEventListener('DOMContentLoaded', accessibility.initialize);
