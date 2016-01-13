// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /**
   * Base class for banners that appear at the top of the settings page.
   */
  function SettingsBannerBase() {}

  cr.addSingletonGetter(SettingsBannerBase);

  SettingsBannerBase.prototype = {
    /**
     * Whether or not the banner has already been dismissed.
     *
     * This is needed because of the surprising ordering of asynchronous
     * JS<->native calls when the settings page is opened with specifying a
     * given sub-page, e.g. chrome://settings/AutomaticSettingsReset.
     *
     * In such a case, AutomaticSettingsResetOverlay's didShowPage(), which
     * calls our dismiss() method, would be called before the native Handlers'
     * InitalizePage() methods have an effect in the JS, which includes calling
     * our show() method. This would mean that the banner would be first
     * dismissed, then shown. We want to prevent this.
     *
     * @type {boolean}
     * @private
     */
    hadBeenDismissed_: false,

    /**
     * Metric name to send when a show event occurs.
     */
    showMetricName_: '',

    /**
     * Name of the native callback invoked when the banner is dismised.
     */
    dismissNativeCallbackName_: '',

    /**
     * DOM element whose visibility is set when setVisibility_ is called.
     */
    setVisibilibyDomElement_: null,

    /**
     * Called by the native code to show the banner if needed.
     * @private
     */
    show_: function() {
      if (!this.hadBeenDismissed_) {
        chrome.send('metricsHandler:recordAction', [this.showMetricName_]);
        this.setVisibility_(true);
      }
    },

    /**
     * Called when the banner should be closed as a result of something taking
     * place on the WebUI page, i.e. when its close button is pressed, or when
     * the confirmation dialog for the profile settings reset feature is opened.
     * @private
     */
    dismiss_: function() {
      chrome.send(this.dismissNativeCallbackName_);
      this.hadBeenDismissed_ = true;
      this.setVisibility_(false);
    },

    /**
     * Sets whether or not the reset profile settings banner shall be visible.
     * @param {boolean} show Whether or not to show the banner.
     * @private
     */
    setVisibility_: function(show) {
      this.setVisibilibyDomElement_.hidden = !show;
    },

  };

  // Export
  return {
    SettingsBannerBase: SettingsBannerBase
  };
});
