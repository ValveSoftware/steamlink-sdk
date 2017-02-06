// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-input` is a single-line text field for user input associated
 * with a pref value.
 */
Polymer({
  is: 'settings-input',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /**
     * The preference object to control.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     * @override
     */
    pref: {
      observer: 'prefChanged_'
    },

    /**
     * The current value of the input, reflected to/from |pref|.
     */
    value: {
      type: String,
      value: '',
      notify: true,
    },

    /**
     * Set to true to disable editing the input.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },

    /** Propagate the errorMessage property. */
    errorMessage: { type: String },

    /** Propagate the label property. */
    label: { type: String },

    /** Propagate the no-label-float property. */
    noLabelFloat: { type: Boolean, value: false },

    /** Propagate the pattern property. */
    pattern: { type: String },

    /** Propagate the readonly property. */
    readonly: { type: Boolean, value: false },

    /** Propagate the required property. */
    required: { type: Boolean, value: false },

    /** Propagate the type property. */
    type: { type: String },
  },

  /** @override */
  ready: function() {
    this.$.events.forward(this.$.input, ['change']);
  },

  /**
   * Focuses the 'input' element.
   */
  focus: function() {
    this.$.input.focus();
  },

  /**
   * Polymer changed observer for |pref|.
   * @private
   */
  prefChanged_: function() {
    if (!this.pref)
      return;

    // Ignore updates while the input is focused so that user input is not
    // overwritten.
    if (this.$.input.focused)
      return;

    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      this.value = this.pref.value.toString();
    } else {
      assert(this.pref.type == chrome.settingsPrivate.PrefType.STRING ||
             this.pref.type == chrome.settingsPrivate.PrefType.URL);
      this.value = /** @type {string} */(this.pref.value);
    }
  },

  /**
   * Blur method for paper-input. Only update the pref value on a blur event.
   * @private
   */
  onBlur_: function() {
    if (!this.pref)
      return;

    if (this.pref.type == chrome.settingsPrivate.PrefType.NUMBER) {
      if (!this.value) {
        // Ignore empty input field and restore value.
        this.value = this.pref.value.toString();
        return;
      }
      var n = parseInt(this.value, 10);
      if (isNaN(n)) {
        console.error('Bad value for numerical pref: ' + this.value);
        return;
      }
      this.set('pref.value', n);
    } else {
      assert(this.pref.type == chrome.settingsPrivate.PrefType.STRING ||
             this.pref.type == chrome.settingsPrivate.PrefType.URL);
      this.set('pref.value', this.value);
    }
  },

  /**
   * @param {boolean} disabled
   * @param {!chrome.settingsPrivate.PrefObject} pref
   * @return {boolean} Whether the element should be disabled.
   * @private
   */
  isDisabled_: function(disabled, pref) {
    return disabled || this.isPrefPolicyControlled(pref);
  },
});
