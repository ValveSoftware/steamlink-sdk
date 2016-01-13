// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

/**
 * WebUI for configuring instant.* preference values used by
 * Chrome's instant search system.
 */
var instantConfig = (function() {
  'use strict';

  /** List of fields used to dynamically build form. **/
  var FIELDS = [
    {
      key: 'instant_ui.zero_suggest_url_prefix',
      label: 'Prefix URL for the experimental Instant ZeroSuggest provider',
      type: 'string',
      size: 40,
      units: '',
      default: ''
    },
  ];

  /**
   * Returns a DOM element of the given type and class name.
   */
  function createElementWithClass(elementType, className) {
    var element = document.createElement(elementType);
    element.className = className;
    return element;
  }

  /**
   * Dynamically builds web-form based on FIELDS list.
   * @return {string} The form's HTML.
   */
  function buildForm() {
    var buf = [];

    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];

      var row = createElementWithClass('div', 'row');
      row.id = '';

      var label = createElementWithClass('label', 'row-label');
      label.setAttribute('for', field.key);
      label.textContent = field.label;
      row.appendChild(label);

      var input = createElementWithClass('input', 'row-input');
      input.type = field.type;
      input.id = field.key;
      input.title = "Default Value: " + field.default;
      if (field.size) input.size = field.size;
      input.min = field.min || 0;
      if (field.max) input.max = field.max;
      if (field.step) input.step = field.step;
      row.appendChild(input);

      var units = createElementWithClass('div', 'row-units');
      if (field.units) units.innerHTML = field.units;
      row.appendChild(units);

      $('instant-form').appendChild(row);
    }
  }

  /**
   * Initialize the form by adding 'onChange' listeners to all fields.
   */
  function initForm() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      $(field.key).onchange = (function(key) {
        setPreferenceValue(key);
      }).bind(null, field.key);
    }
  }

  /**
   * Request a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * getPreferenceValueResult.
   * @param {string} prefName The name of the preference value being requested.
   */
  function getPreferenceValue(prefName) {
    chrome.send('getPreferenceValue', [prefName]);
  }

  /**
   * Handle callback from call to getPreferenceValue.
   * @param {string} prefName The name of the requested preference value.
   * @param {value} value The current value associated with prefName.
   */
  function getPreferenceValueResult(prefName, value) {
    if ($(prefName).type == 'checkbox')
      $(prefName).checked = value;
    else
      $(prefName).value = value;
  }

  /**
   * Set a preference setting's value stored in the element with prefName.
   * @param {string} prefName The name of the preference value being set.
   */
  function setPreferenceValue(prefName) {
    var value;
    if ($(prefName).type == 'checkbox')
      value = $(prefName).checked;
    else if ($(prefName).type == 'number')
      value = parseFloat($(prefName).value);
    else
      value = $(prefName).value;
    chrome.send('setPreferenceValue', [prefName, value]);
  }

  /**
   * Saves data back into Chrome preferences.
   */
  function onSave() {
    for (var i = 0; i < FIELDS.length; i++) {
      var field = FIELDS[i];
      setPreferenceValue(field.key);
    }
    return false;
  }

  /**
   * Request debug info.
   * The method is asynchronous, results being provided via getDebugInfoResult.
   */
  function getDebugInfo() {
    chrome.send('getDebugInfo');
  }

  /**
   * Handles callback from getDebugInfo.
   * @param {Object} info The debug info.
   */
  function getDebugInfoResult(info) {
    for (var i = 0; i < info.entries.length; ++i) {
      var entry = info.entries[i];
      var row = createElementWithClass('p', 'debug');
      row.appendChild(createElementWithClass('span', 'timestamp')).textContent =
          entry.time;
      row.appendChild(document.createElement('span')).textContent = entry.text;
      $('instant-debug-info').appendChild(row);
    }
  }

  /**
   * Resets list of debug events.
   */
  function clearDebugInfo() {
    $('instant-debug-info').innerHTML = '';
    chrome.send('clearDebugInfo');
  }

  function loadForm() {
    for (var i = 0; i < FIELDS.length; i++)
      getPreferenceValue(FIELDS[i].key);
  }

  /**
   * Build and initialize the configuration form.
   */
  function initialize() {
    buildForm();
    loadForm();
    initForm();
    getDebugInfo();

    $('save-button').onclick = onSave.bind(this);
    $('clear-button').onclick = clearDebugInfo.bind(this);
  }

  return {
    initialize: initialize,
    getDebugInfoResult: getDebugInfoResult,
    getPreferenceValueResult: getPreferenceValueResult
  };
})();

document.addEventListener('DOMContentLoaded', instantConfig.initialize);
