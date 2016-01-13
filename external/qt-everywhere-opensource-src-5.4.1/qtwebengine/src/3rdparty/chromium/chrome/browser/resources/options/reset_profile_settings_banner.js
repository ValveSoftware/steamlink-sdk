// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: the native-side handler for this is ResetProfileSettingsHandler.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var SettingsBannerBase = options.SettingsBannerBase;

  /**
   * ResetProfileSettingsBanner class
   * Provides encapsulated handling of the Reset Profile Settings banner.
   * @constructor
   */
  function ResetProfileSettingsBanner() {}

  cr.addSingletonGetter(ResetProfileSettingsBanner);

  ResetProfileSettingsBanner.prototype = {
    __proto__: SettingsBannerBase.prototype,

    /**
     * Initializes the banner's event handlers.
     */
    initialize: function() {
      this.showMetricName_ = 'AutomaticReset_WebUIBanner_BannerShown';

      this.dismissNativeCallbackName_ =
          'onDismissedResetProfileSettingsBanner';

      this.setVisibilibyDomElement_ = $('reset-profile-settings-banner');

      $('reset-profile-settings-banner-close').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticReset_WebUIBanner_ManuallyClosed']);
        ResetProfileSettingsBanner.dismiss();
      };
      $('reset-profile-settings-banner-activate').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticReset_WebUIBanner_ResetClicked']);
        OptionsPage.navigateToPage('resetProfileSettings');
      };
    },
  };

  // Forward public APIs to private implementations.
  [
    'show',
    'dismiss',
  ].forEach(function(name) {
    ResetProfileSettingsBanner[name] = function() {
      var instance = ResetProfileSettingsBanner.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ResetProfileSettingsBanner: ResetProfileSettingsBanner
  };
});
