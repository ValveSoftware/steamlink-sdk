// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: the native-side handler for this is AutomaticSettingsResetHandler.

cr.define('options', function() {
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * AutomaticSettingsResetBanner class
   * Provides encapsulated handling of the Reset Profile Settings banner.
   * @constructor
   */
  function AutomaticSettingsResetBanner() {}

  cr.addSingletonGetter(AutomaticSettingsResetBanner);

  AutomaticSettingsResetBanner.prototype = {
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
     * @private {boolean}
     */
    wasDismissed_: false,

    /**
     * Metric name to send when a show event occurs.
     * @private {string}
     */
    showMetricName_: '',

    /**
     * Name of the native callback invoked when the banner is dismised.
     */
    dismissNativeCallbackName_: '',

    /**
     * DOM element whose visibility is set when setVisibility_ is called.
     * @private {?HTMLElement}
     */
    visibleElement_: null,

    /**
     * Initializes the banner's event handlers.
     * @suppress {checkTypes}
     * TODO(vitalyp): remove the suppression. Suppression is needed because
     * method dismiss() is attached to AutomaticSettingsResetBanner at run-time
     * via "Forward public APIs to protected implementations" pattern (see
     * below). Currently the compiler pass and cr.js handles only forwarding to
     * private implementations using cr.makePublic().
     */
    initialize: function() {
      this.showMetricName_ = 'AutomaticSettingsReset_WebUIBanner_BannerShown';

      this.dismissNativeCallbackName_ =
          'onDismissedAutomaticSettingsResetBanner';

      this.visibleElement_ = getRequiredElement(
          'automatic-settings-reset-banner');

      $('automatic-settings-reset-banner-close').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_ManuallyClosed']);
        AutomaticSettingsResetBanner.dismiss();
      };
      $('automatic-settings-reset-learn-more').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_LearnMoreClicked']);
      };
      $('automatic-settings-reset-banner-activate-reset').onclick =
          function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_ResetClicked']);
        PageManager.showPageByName('resetProfileSettings');
      };
    },

    /**
     * Sets whether or not the reset profile settings banner shall be visible.
     * @param {boolean} show Whether or not to show the banner.
     * @protected
     */
    setVisibility: function(show) {
      this.visibleElement_.hidden = !show;
    },

    /**
     * Called by the native code to show the banner if needed.
     * @private
     */
    show_: function() {
      if (!this.wasDismissed_) {
        chrome.send('metricsHandler:recordAction', [this.showMetricName_]);
        this.setVisibility(true);
      }
    },

    /**
     * Called when the banner should be closed as a result of something taking
     * place on the WebUI page, i.e. when its close button is pressed, or when
     * the confirmation dialog for the profile settings reset feature is opened.
     * @private
     */
    dismiss_: function() {
      chrome.send(assert(this.dismissNativeCallbackName_));
      this.wasDismissed_ = true;
      this.setVisibility(false);
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(AutomaticSettingsResetBanner, [
    'show',
    'dismiss',
  ]);

  // Export
  return {
    AutomaticSettingsResetBanner: AutomaticSettingsResetBanner
  };
});
