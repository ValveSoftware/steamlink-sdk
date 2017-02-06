// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-checkbox` is a checkbox that controls a supplied preference.
 *
 * Example:
 *      <settings-checkbox pref="{{prefs.settings.enableFoo}}"
 *          label="Enable foo setting." subLabel="(bar also)">
 *      </settings-checkbox>
 */
Polymer({
  is: 'settings-checkbox',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /** Whether the checkbox should represent the inverted value. */
    inverted: {
      type: Boolean,
      value: false,
    },

    /** Whether the checkbox is checked. */
    checked: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'checkedChanged_',
      reflectToAttribute: true
    },

    /** Disabled property for the element. */
    disabled: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true
    },

    /** Checkbox label. */
    label: {
      type: String,
      value: '',
    },

    /** Additional sub-label for the checkbox. */
    subLabel: {
      type: String,
      value: '',
    },
  },

  observers: [
    'prefValueChanged_(pref.value)'
  ],

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.checkbox, ['change']);
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
    if (!this.pref)
      return;
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
  checkboxDisabled_: function(disabled, pref) {
    return disabled || this.isPrefPolicyControlled(pref);
  },
});
