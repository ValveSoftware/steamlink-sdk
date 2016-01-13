// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: the native-side handler for this is AutomaticSettingsResetHandler.

cr.define('options', function() {
  /** @const */ var SettingsBannerBase = options.SettingsBannerBase;

  /**
   * AutomaticSettingsResetBanner class
   * Provides encapsulated handling of the Reset Profile Settings banner.
   * @constructor
   */
  function AutomaticSettingsResetBanner() {}

  cr.addSingletonGetter(AutomaticSettingsResetBanner);

  AutomaticSettingsResetBanner.prototype = {
    __proto__: SettingsBannerBase.prototype,

    /**
     * Initializes the banner's event handlers.
     */
    initialize: function() {
      this.showMetricName_ = 'AutomaticSettingsReset_WebUIBanner_BannerShown';

      this.dismissNativeCallbackName_ =
          'onDismissedAutomaticSettingsResetBanner';

      this.setVisibilibyDomElement_ = $('automatic-settings-reset-banner');

      $('automatic-settings-reset-banner-close').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_ManuallyClosed']);
        AutomaticSettingsResetBanner.dismiss();
      };
      $('automatic-settings-reset-learn-more').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_LearnMoreClicked']);
      };
    },
  };

  // Forward public APIs to private implementations.
  [
    'show',
    'dismiss',
  ].forEach(function(name) {
    AutomaticSettingsResetBanner[name] = function() {
      var instance = AutomaticSettingsResetBanner.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    AutomaticSettingsResetBanner: AutomaticSettingsResetBanner
  };
});
