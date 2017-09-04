// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A behavior to help controls that handle a boolean preference, such as
 * checkbox and toggle button.
 */

/** @polymerBehavior SettingsBooleanControlBehavior */
var SettingsBooleanControlBehaviorImpl = {
  properties: {
    /** Whether the control should represent the inverted value. */
    inverted: {
      type: Boolean,
      value: false,
    },

    /** Whether the control is checked. */
    checked: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'checkedChanged_',
      reflectToAttribute: true,
    },

    /** Disabled property for the element. */
    disabled: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
    },

    /**
     * If true, do not automatically set the preference value. This allows the
     * container to confirm the change first then call either sendPrefChange
     * or resetToPrefValue accordingly.
     */
    noSetPref: {
      type: Boolean,
      value: false,
    },

    /** The main label. */
    label: {
      type: String,
      value: '',
    },

    /** Additional (optional) sub-label. */
    subLabel: {
      type: String,
      value: '',
    },
  },

  observers: [
    'prefValueChanged_(pref.value)',
  ],

  /** Reset the checked state to match the current pref value. */
  resetToPrefValue: function() {
    this.checked = this.getNewValue_(this.pref.value);
  },

  /** Update the pref to the current |checked| value. */
  sendPrefChange: function() {
    /** @type {boolean} */ var newValue = this.getNewValue_(this.checked);
    // Ensure that newValue is the correct type for the pref type, either
    // a boolean or a number.
    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      this.set('pref.value', newValue ? 1 : 0);
      return;
    }
    this.set('pref.value', newValue);
  },

  /**
   * Polymer observer for pref.value.
   * @param {*} prefValue
   * @private
   */
  prefValueChanged_: function(prefValue) {
    this.checked = this.getNewValue_(prefValue);
  },

  /**
   * Polymer observer for checked.
   * @private
   */
  checkedChanged_: function() {
    if (!this.pref || this.noSetPref)
      return;
    this.sendPrefChange();
  },

  /**
   * @param {*} value
   * @return {boolean} The value as a boolean, inverted if |inverted| is true.
   * @private
   */
  getNewValue_: function(value) {
    return this.inverted ? !value : !!value;
  },

  /**
   * @param {boolean} disabled
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether the checkbox should be disabled.
   * @private
   */
  controlDisabled_: function(disabled, pref) {
    return disabled || this.isPrefPolicyControlled(pref);
  },
};

/** @polymerBehavior */
var SettingsBooleanControlBehavior = [
  CrPolicyPrefBehavior,
  PrefControlBehavior,
  SettingsBooleanControlBehaviorImpl,
];
