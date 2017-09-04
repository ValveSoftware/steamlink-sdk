// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-reset-page prefs="{{prefs}}">
 *      </settings-reset-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-reset-page',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
<if expr="chromeos">
    /** @private */
    showPowerwashDialog_: Boolean,
</if>

    /** @private */
    allowPowerwash_: {
      type: Boolean,
      value: cr.isChromeOS ? loadTimeData.getBoolean('allowPowerwash') : false
    },

    /** @private */
    showResetProfileDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    this.showResetProfileDialog_ =
        route == settings.Route.TRIGGERED_RESET_DIALOG ||
        route == settings.Route.RESET_DIALOG;
  },

  /** @private */
  onShowResetProfileDialog_: function() {
    settings.navigateTo(settings.Route.RESET_DIALOG,
                        new URLSearchParams('origin=userclick'));
  },

  /** @private */
  onResetProfileDialogClose_: function() {
    settings.navigateToPreviousRoute();
  },

<if expr="chromeos">
  /** @private */
  onShowPowerwashDialog_: function() {
    this.showPowerwashDialog_ = true;
  },

  /** @private */
  onPowerwashDialogClose_: function() {
    this.showPowerwashDialog_ = false;
  },
</if>
});
