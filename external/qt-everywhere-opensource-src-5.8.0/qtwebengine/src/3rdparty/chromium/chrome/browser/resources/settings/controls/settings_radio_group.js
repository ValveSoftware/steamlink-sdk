// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-radio-group` wraps a radio-group and set of radio-buttons that control
 *  a supplied preference.
 *
 * Example:
 *      <settings-radio-group pref="{{prefs.settings.foo}}"
 *          label="Foo Options." buttons="{{fooOptionsList}}">
 *      </settings-radio-group>
 */
Polymer({
  is: 'settings-radio-group',

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    disabled_: {
      observer: 'disabledChanged_',
      type: Boolean,
      value: false,
    },

    /**
     * IronSelectableBehavior selected attribute.
     */
    selected: {
      type: String,
      notify: true,
      observer: 'selectedChanged_'
    },
  },

  observers: [
    'prefChanged_(pref.*)',
  ],

  /** @private */
  prefChanged_: function() {
    var pref = /** @type {!chrome.settingsPrivate.PrefObject} */(this.pref);
    this.disabled_ = this.isPrefPolicyControlled(pref);
    this.selected = Settings.PrefUtil.prefToString(pref);
  },

  /** @private */
  disabledChanged_: function() {
    var radioButtons = this.queryAllEffectiveChildren('paper-radio-button');
    for (var i = 0; i < radioButtons.length; ++i) {
      radioButtons[i].disabled = this.disabled_;
    }
  },

  /** @private */
  selectedChanged_: function(selected) {
    if (!this.pref)
      return;
    this.set('pref.value',
             Settings.PrefUtil.stringToPrefValue(selected, this.pref));
  },
});
