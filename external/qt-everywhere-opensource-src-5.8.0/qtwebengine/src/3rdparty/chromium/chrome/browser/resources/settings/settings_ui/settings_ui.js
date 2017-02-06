// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
Polymer({
  is: 'settings-ui',

  properties: {
    /**
     * Preferences state.
     * @type {?CrSettingsPrefsElement}
     */
    prefs: Object,

    /** @type {?settings.DirectionDelegate} */
    directionDelegate: {
      observer: 'directionDelegateChanged_',
      type: Object,
    },

    appealClosed_: {
      type: Boolean,
      value: function() {
        return !!(sessionStorage.appealClosed_ || localStorage.appealClosed_);
      },
    },
  },

  listeners: {
    'sideNav.iron-activate': 'onIronActivate_',
  },

  /** @private */
  onCloseAppealTap_: function() {
    sessionStorage.appealClosed_ = this.appealClosed_ = true;
  },

  /**
   * @param {Event} event
   * @private
   */
  onIronActivate_: function(event) {
    if (event.detail.item.id != 'advancedPage')
      this.$$('app-drawer').close();
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.$$('app-drawer').toggle();
  },

  /** @private */
  directionDelegateChanged_: function() {
    this.$$('app-drawer').align = this.directionDelegate.isRtl() ?
        'right' : 'left';
  },
});
