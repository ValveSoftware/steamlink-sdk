// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

var currentTreatment = 0;
var treatments = [];

/**
 * Take a string of hex like '74657374' and return the ascii version 'test'.
 * @param {string} str The string of hex characters to convert to ascii
 * @return {string} The ASCII values of those hex encoded characters
 */
function hexToChars(str) {
  var decoded = '';
  if (str.length % 2 == 0) {
    for (var pos = str.length; pos > 0; pos = pos - 2) {
      var c = String.fromCharCode(parseInt(str.substring(pos - 2, pos), 16));
      decoded = c + decoded;
    }
  }
  return decoded;
}

/**
 * Extract the experiment information out of the encoded URL string.
 * The format is as follows:
 * chrome://salsa/#HEX_ENCODED_EXPERIMENT
 * Experiments are encoded as:
 *    treatment1+treatment2+...+treatmentn
 * Each treatment is of the form:
 *    preference1,preference2,...,preferencen
 * Each preference is of the form:
 *    name:value
 * This function returns an object storing all the parsed data.
 * @param {string} url The URL to parse the experiment from
 * @return {list} a list of objects, each representing a single treatment
 *     and consisting of a set of preference name -> value pairs
 */
function parseURL(url) {
  var match = url.match('#([0-9ABCDEFabcdef]*)');
  var experimentString = match ? match[1] : '';
  experimentString = hexToChars(experimentString);

  var treatmentsFound = [];
  if (experimentString == '')
    return treatmentsFound;

  var treatmentStrings = experimentString.split('+');
  for (var i = 0; i < treatmentStrings.length; i++) {
    var prefStrings = treatmentStrings[i].split(',');
    treatment = [];
    for (var j = 0; j < prefStrings.length; j++) {
      var key = prefStrings[j].split(':')[0];
      var value = prefStrings[j].split(':')[1];
      treatment.push({'key': key, 'value': value});
    }
    treatmentsFound.push(treatment);
  }

  return treatmentsFound;
}

function setPreferenceValue(key, value) {
  chrome.send('salsaSetPreferenceValue', [key, parseFloat(value)]);
}

function backupPreferenceValue(key) {
  chrome.send('salsaBackupPreferenceValue', [key]);
}

function handleKeyPress(e) {
  e = e || window.event;
  var selectedTreatment = currentTreatment;

  if (e.keyCode == '37' || e.keyCode == '38') {
    selectedTreatment = currentTreatment - 1;
    if (selectedTreatment < 0)
      selectedTreatment = 0;
  } else if (e.keyCode == '39' || e.keyCode == '40') {
    selectedTreatment = currentTreatment + 1;
    if (selectedTreatment >= treatments.length)
      selectedTreatment = treatments.length - 1;
  }

  if (selectedTreatment != currentTreatment)
    applyTreatment(selectedTreatment);
}

function applyTreatment(treatment_number) {
  if (treatment_number < 0)
    treatment_number = 0;
  if (treatment_number >= treatments.length)
    treatment_number = treatments.length;

  $('treatment' + currentTreatment).className = 'treatment';
  currentTreatment = treatment_number;
  $('treatment' + currentTreatment).className = 'selected treatment';

  for (var i = 0; i < treatments[treatment_number].length; i++) {
    var key = treatments[treatment_number][i].key;
    var value = treatments[treatment_number][i].value;
    setPreferenceValue(key, value);
  }
}

function initialize() {
  // Parse the experiment string in the URL.
  treatments = parseURL(document.URL);

  // Update the available treatments list.
  for (var i = 0; i < treatments.length; i++) {
    var newTreatment = $('treatment-template').cloneNode(true);
    newTreatment.id = 'treatment' + i.toString();
    newTreatment.removeAttribute('hidden');
    newTreatment.onclick = function() {
      applyTreatment(parseInt(this.textContent));
    };
    newTreatment.textContent = i.toString();
    $('treatment-list').appendChild(newTreatment);
  }

  if (treatments.length > 0) {
    // Store a copy of the settings right now so you can reset them afterwards.
    for (var i = 0; i < treatments[0].length; i++)
      backupPreferenceValue(treatments[0][i].key);

    // Select Treatment 0 to start
    applyTreatment(0);
  } else {
    // Make the error message visible and hide everything else
    $('invalid_treatment_info').removeAttribute('hidden');
    var div = $('valid_treatment_info');
    div.parentNode.removeChild(div);
  }
}

/**
 * A key handler so the user can use the arrow keys to select their treatments.
 * This should fire any time they press a key
 */
document.onkeydown = handleKeyPress;
document.addEventListener('DOMContentLoaded', initialize);
