// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function ResetBrowserProxy() {}

  ResetBrowserProxy.prototype = {
    /**
     * @param {boolean} sendSettings Whether the user gave consent to upload
     *     broken settings to Google for analysis.
     * @return {!Promise} A promise firing once resetting has completed.
     */
    performResetProfileSettings: function(sendSettings) {},

    /**
     * A method to be called when the reset profile dialog is hidden.
     */
    onHideResetProfileDialog: function() {},

    /**
     * A method to be called when the reset profile banner is hidden.
     */
    onHideResetProfileBanner: function() {},

    /**
     * A method to be called when the reset profile dialog is shown.
     */
    onShowResetProfileDialog: function() {},

    /**
     * Shows the settings that are about to be reset and which will be reported
     * to Google for analysis, in a new tab.
     */
    showReportedSettings: function() {},

<if expr="chromeos">
    /**
     * A method to be called when the reset powerwash dialog is shown.
     */
    onPowerwashDialogShow: function() {},

    /**
     * Initiates a factory reset and restarts ChromeOS.
     */
    requestFactoryResetRestart: function() {},
</if>
  };

  /**
   * @constructor
   * @implements {settings.ResetBrowserProxy}
   */
  function ResetBrowserProxyImpl() {}
  cr.addSingletonGetter(ResetBrowserProxyImpl);

  ResetBrowserProxyImpl.prototype = {
    /** @override */
    performResetProfileSettings: function(sendSettings) {
      return cr.sendWithPromise('performResetProfileSettings', sendSettings);
    },

    /** @override */
    onHideResetProfileDialog: function() {
      chrome.send('onHideResetProfileDialog');
    },

    /** @override */
    onHideResetProfileBanner: function() {
      chrome.send('onHideResetProfileBanner');
    },

    /** @override */
    onShowResetProfileDialog: function() {
      chrome.send('onShowResetProfileDialog');
    },

    /** @override */
    showReportedSettings: function() {
      cr.sendWithPromise('getReportedSettings').then(function(settings) {
        var output = settings.map(function(entry) {
          return entry.key + ': ' + entry.value.replace(/\n/g, ', ');
        });
        var win = window.open('about:blank');
        var div = win.document.createElement('div');
        div.textContent = output.join('\n');
        div.style.whiteSpace = 'pre';
        win.document.body.appendChild(div);
      });
    },

<if expr="chromeos">
    /** @override */
    onPowerwashDialogShow: function() {
      chrome.send('onPowerwashDialogShow');
    },

    /** @override */
    requestFactoryResetRestart: function() {
      chrome.send('requestFactoryResetRestart');
    },
</if>
  };

  return {
    ResetBrowserProxyImpl: ResetBrowserProxyImpl,
  };
});
