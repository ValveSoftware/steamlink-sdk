// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;

  var ResetProfileSettingsOverlay = options.ResetProfileSettingsOverlay;

  /**
   * TriggeredResetProfileSettingsOverlay class
   * Encapsulated handling of the triggered variant of the 'Reset Profile
   * Settings' overlay page. Identical to ResetProfileSettingsOverlay but in a
   * new class to get a new overlay url mapping. See
   * triggered_profile_resetter.h for when this will be used.
   * @constructor
   * @extends {options.ResetProfileSettingsOverlay}
   */
  function TriggeredResetProfileSettingsOverlay() {
    // Note here that 'reset-profile-settings-overlay' is intentionally used as
    // the pageDivName argument to reuse the layout and CSS from the reset
    // profile settings overlay defined in reset_profile_settings_overlay.js.
    Page.call(this, 'triggeredResetProfileSettings',
              loadTimeData.getString('triggeredResetProfileSettingsOverlay'),
              'reset-profile-settings-overlay');
  }

  cr.addSingletonGetter(TriggeredResetProfileSettingsOverlay);

  TriggeredResetProfileSettingsOverlay.prototype = {
    __proto__: ResetProfileSettingsOverlay.prototype,

    /** @override */
    didShowPage: function() {
      $('reset-profile-settings-title').textContent =
          loadTimeData.getString('triggeredResetProfileSettingsOverlay');
      $('reset-profile-settings-explanation').textContent =
          loadTimeData.getString('triggeredResetProfileSettingsExplanation');
      chrome.send('onShowResetProfileDialog');
    },
  };

  // Export
  return {
    TriggeredResetProfileSettingsOverlay: TriggeredResetProfileSettingsOverlay
  };
});
