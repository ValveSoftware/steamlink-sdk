// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to interact
 * with the Easy Unlock functionality of the browser. ChromeOS only.
 */
cr.exportPath('settings');

cr.define('settings', function() {
  /** @interface */
  function EasyUnlockBrowserProxy() {}

  EasyUnlockBrowserProxy.prototype = {
    /**
     * Returns a true promise if Easy Unlock is already enabled on the device.
     * @return {!Promise<boolean>}
     */
    getEnabledStatus: function() {},

    /**
     * Starts the Easy Unlock setup flow.
     */
    startTurnOnFlow: function() {},

    /**
     * Returns the Easy Unlock turn off flow status.
     * @return {!Promise<string>}
     */
    getTurnOffFlowStatus: function() {},

    /**
     * Begins the Easy Unlock turn off flow.
     */
    startTurnOffFlow: function() {},

    /**
     * Cancels any in-progress Easy Unlock turn-off flows.
     */
    cancelTurnOffFlow: function() {},
  };

  /**
   * @constructor
   * @implements {settings.EasyUnlockBrowserProxy}
   */
  function EasyUnlockBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(EasyUnlockBrowserProxyImpl);

  EasyUnlockBrowserProxyImpl.prototype = {
    /** @override */
    getEnabledStatus: function() {
      return cr.sendWithPromise('easyUnlockGetEnabledStatus');
    },

    /** @override */
    startTurnOnFlow: function() {
      chrome.send('easyUnlockStartTurnOnFlow');
    },

    /** @override */
    getTurnOffFlowStatus: function() {
      return cr.sendWithPromise('easyUnlockGetTurnOffFlowStatus');
    },

    /** @override */
    startTurnOffFlow: function() {
      chrome.send('easyUnlockStartTurnOffFlow');
    },

    /** @override */
    cancelTurnOffFlow: function() {
      chrome.send('easyUnlockCancelTurnOffFlow');
    },
  };

  return {
    EasyUnlockBrowserProxy: EasyUnlockBrowserProxy,
    EasyUnlockBrowserProxyImpl: EasyUnlockBrowserProxyImpl,
  };
});
