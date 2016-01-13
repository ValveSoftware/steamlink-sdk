// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Redefine '$' here rather than including 'cr.js', since this is
// the only function needed.  This allows this file to be loaded
// in a browser directly for layout and some testing purposes.
var $ = function(id) { return document.getElementById(id); };

/**
 * A generic WebUI for configuring preference values used by Chrome's gesture
 * recognition systems.
 * @param {string} title The user-visible title to display for the configuration
 *    section.
 * @param {string} prefix The prefix for the configuration fields.
 * @param {!Object} fields An array of fields that contain the name of the pref
 *    and user-visible labels.
 */
function GeneralConfig(title, prefix, fields) {
  this.title = title;
  this.prefix = prefix;
  this.fields = fields;
}

GeneralConfig.prototype = {
  /**
   * Sets up the form for configuring all the preference values.
   */
  buildAll: function() {
    this.buildForm();
    this.loadForm();
    this.initForm();
  },

  /**
   * Dynamically builds web-form based on the list of preferences.
   */
  buildForm: function() {
    var buf = [];

    var section = $('section-template').cloneNode(true);
    section.removeAttribute('id');
    var title = section.querySelector('.section-title');
    title.textContent = this.title;

    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];

      var row = $('section-row-template').cloneNode(true);
      row.removeAttribute('id');

      var label = row.querySelector('.row-label');
      var input = row.querySelector('.input');
      var units = row.querySelector('.row-units');
      var reset = row.querySelector('.row-reset');

      label.setAttribute('for', field.key);
      label.innerHTML = field.label;
      input.id = field.key;
      input.min = field.min || 0;

      if (field.max)
        input.max = field.max;

      input.step = field.step || 'any';

      if (field.units)
        units.innerHTML = field.units;

      reset.id = field.key + '-reset';
      gesture_config.updateResetButton(reset, true);

      section.querySelector('.section-properties').appendChild(row);
    }
    $('gesture-form').appendChild(section);
  },

  /**
   * Initializes the form by adding appropriate event listeners to elements.
   */
  initForm: function() {
    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];
      var config = this;
      $(field.key).onchange = (function(key) {
        config.setPreferenceValue(key, $(key).value);
        gesture_config.updateResetButton($(key + '-reset'), false);
        gesture_config.updateResetAllButton(false);
      }).bind(null, field.key);
      $(field.key + '-reset').onclick = (function(key) {
        config.resetPreferenceValue(key);
      }).bind(null, field.key);
    }
  },

  /**
   * Requests preference values for all the relevant fields.
   */
  loadForm: function() {
    for (var i = 0; i < this.fields.length; i++)
      this.updatePreferenceValue(this.fields[i].key);
  },

  /**
   * Handles processing of "Reset All" button.
   * Causes all form values to be updated based on current preference values.
   * @return {boolean} Returns false.
   */
  onReset: function() {
    for (var i = 0; i < this.fields.length; i++) {
      var field = this.fields[i];
      this.resetPreferenceValue(field.key);
    }
    return false;
  },

  /**
   * Requests a preference setting's value.
   * This method is asynchronous; the result is provided by a call to
   * updatePreferenceValueResult.
   * @param {string} prefName The name of the preference value being requested.
   */
  updatePreferenceValue: function(prefName) {
    chrome.send('updatePreferenceValue', [this.prefix + prefName]);
  },

  /**
   * Sets a preference setting's value.
   * @param {string} prefName The name of the preference value being set.
   * @param {value} value The value to be associated with prefName.
   */
  setPreferenceValue: function(prefName, value) {
    chrome.send('setPreferenceValue',
        [this.prefix + prefName, parseFloat(value)]);
  },

  /**
   * Resets a preference to its default value and get that callback
   * to updatePreferenceValueResult with the new value of the preference.
   * @param {string} prefName The name of the requested preference.
   */
  resetPreferenceValue: function(prefName) {
    chrome.send('resetPreferenceValue', [this.prefix + prefName]);
  }
};

/**
 * Returns a GeneralConfig for configuring gestures.* preferences.
 * @return {object} A GeneralConfig object.
 */
function GestureConfig() {
  /** The title of the section for the gesture preferences. **/
  /** @const */ var GESTURE_TITLE = 'Gesture Configuration';

  /** Common prefix of gesture preferences. **/
  /** @const */ var GESTURE_PREFIX = 'gesture.';

  /** List of fields used to dynamically build form. **/
  var GESTURE_FIELDS = [
    {
      key: 'fling_max_cancel_to_down_time_in_ms',
      label: 'Maximum Cancel to Down Time for Tap Suppression',
      units: 'milliseconds',
    },
    {
      key: 'fling_max_tap_gap_time_in_ms',
      label: 'Maximum Tap Gap Time for Tap Suppression',
      units: 'milliseconds',
    },
    {
      key: 'semi_long_press_time_in_seconds',
      label: 'Semi Long Press Time',
      units: 'seconds',
      step: 0.1
    },
    {
      key: 'max_separation_for_gesture_touches_in_pixels',
      label: 'Maximum Separation for Gesture Touches',
      units: 'pixels'
    },
    {
      key: 'fling_acceleration_curve_coefficient_0',
      label: 'Touchscreen Fling Acceleration',
      units: 'x<sup>3</sup>',
      min: '-1'
    },
    {
      key: 'fling_acceleration_curve_coefficient_1',
      label: '+',
      units: 'x<sup>2</sup>',
      min: '-1'
    },
    {
      key: 'fling_acceleration_curve_coefficient_2',
      label: '+',
      units: 'x<sup>1</sup>',
      min: '-1'
    },
    {
      key: 'fling_acceleration_curve_coefficient_3',
      label: '+',
      units: 'x<sup>0</sup>',
      min: '-1'
    },
    {
      key: 'tab_scrub_activation_delay_in_ms',
      label: 'Tab scrub auto activation delay, (-1 for never)',
      units: 'milliseconds'
    }
  ];

  return new GeneralConfig(GESTURE_TITLE, GESTURE_PREFIX, GESTURE_FIELDS);
}

/**
 * Returns a GeneralConfig for configuring overscroll.* preferences.
 * @return {object} A GeneralConfig object.
 */
function OverscrollConfig() {
  /** @const */ var OVERSCROLL_TITLE = 'Overscroll Configuration';

  /** @const */ var OVERSCROLL_PREFIX = 'overscroll.';

  var OVERSCROLL_FIELDS = [
    {
      key: 'horizontal_threshold_complete',
      label: 'Complete when overscrolled (horizontal)',
      units: '%'
    },
    {
      key: 'vertical_threshold_complete',
      label: 'Complete when overscrolled (vertical)',
      units: '%'
    },
    {
      key: 'minimum_threshold_start_touchpad',
      label: 'Start overscroll gesture (horizontal; touchpad)',
      units: 'pixels'
    },
    {
      key: 'minimum_threshold_start',
      label: 'Start overscroll gesture (horizontal; touchscreen)',
      units: 'pixels'
    },
    {
      key: 'vertical_threshold_start',
      label: 'Start overscroll gesture (vertical)',
      units: 'pixels'
    },
    {
      key: 'horizontal_resist_threshold',
      label: 'Start resisting overscroll after (horizontal)',
      units: 'pixels'
    },
    {
      key: 'vertical_resist_threshold',
      label: 'Start resisting overscroll after (vertical)',
      units: 'pixels'
    },
  ];

  return new GeneralConfig(OVERSCROLL_TITLE,
                           OVERSCROLL_PREFIX,
                           OVERSCROLL_FIELDS);
}

/**
 * Returns a GeneralConfig for configuring flingcurve.* preferences.
 * @return {object} A GeneralConfig object.
 */
function FlingConfig() {
  /** @const */ var FLING_TITLE = 'Fling Configuration';

  /** @const */ var FLING_PREFIX = 'flingcurve.';

  var FLING_FIELDS = [
    {
      key: 'touchscreen_alpha',
      label: 'Touchscreen fling deacceleration coefficients',
      units: 'alpha',
      min: '-inf'
    },
    {
      key: 'touchscreen_beta',
      label: '',
      units: 'beta',
      min: '-inf'
    },
    {
      key: 'touchscreen_gamma',
      label: '',
      units: 'gamma',
      min: '-inf'
    },
    {
      key: 'touchpad_alpha',
      label: 'Touchpad fling deacceleration coefficients',
      units: 'alpha',
      min: '-inf'
    },
    {
      key: 'touchpad_beta',
      label: '',
      units: 'beta',
      min: '-inf'
    },
    {
      key: 'touchpad_gamma',
      label: '',
      units: 'gamma',
      min: '-inf'
    },
  ];

  return new GeneralConfig(FLING_TITLE, FLING_PREFIX, FLING_FIELDS);
}

/**
 * WebUI instance for configuring preference values related to gesture input.
 */
window.gesture_config = {
  /**
   * Build and initialize the gesture configuration form.
   */
  initialize: function() {
    var g = GestureConfig();
    g.buildAll();

    var o = OverscrollConfig();
    o.buildAll();

    var f = FlingConfig();
    f.buildAll();

    $('reset-all-button').onclick = function() {
      g.onReset();
      o.onReset();
      f.onReset();
    };
  },

  /**
   * Checks if all gesture preferences are set to default by checking the status
   * of the reset button associated with each preference.
   * @return {boolean} True if all gesture preferences are set to default.
   */
  areAllPrefsSetToDefault: function() {
    var resets = $('gesture-form').querySelectorAll('.row-reset');
    for (var i = 0; i < resets.length; i++) {
      if (!resets[i].disabled)
        return false;
    }
    return true;
  },

  /**
   * Updates the status and label of a preference reset button.
   * @param {HTMLInputElement} resetButton Reset button for the preference.
   * @param {boolean} isDefault Whether the preference is set to the default
   *     value.
   */
  updateResetButton: function(resetButton, isDefault) {
    /** @const */ var TITLE_DEFAULT = 'Default';

    /** @const */ var TITLE_NOT_DEFAULT = 'Reset';

    resetButton.innerHTML = isDefault ? TITLE_DEFAULT : TITLE_NOT_DEFAULT;
    resetButton.disabled = isDefault;
  },

  /**
   * Updates the status and label of "Reset All" button.
   * @param {boolean} isDefault Whether all preference are set to their default
   *     values.
   */
  updateResetAllButton: function(isDefault) {
    /** @const */ var TITLE_DEFAULT = 'Everything is set to default';

    /** @const */ var TITLE_NOT_DEFAULT = 'Reset All To Default';

    var button = $('reset-all-button');
    button.innerHTML = isDefault ? TITLE_DEFAULT : TITLE_NOT_DEFAULT;
    button.disabled = isDefault;
  },

  /**
   * Handle callback from call to updatePreferenceValue.
   * @param {string} prefName The name of the requested preference value.
   * @param {value} value The current value associated with prefName.
   * @param {boolean} isDefault Whether the value is the default value.
   */
  updatePreferenceValueResult: function(prefName, value, isDefault) {
    prefName = prefName.substring(prefName.indexOf('.') + 1);
    $(prefName).value = value;
    this.updateResetButton($(prefName + '-reset'), isDefault);
    this.updateResetAllButton(this.areAllPrefsSetToDefault());
  },
};

document.addEventListener('DOMContentLoaded', gesture_config.initialize);
