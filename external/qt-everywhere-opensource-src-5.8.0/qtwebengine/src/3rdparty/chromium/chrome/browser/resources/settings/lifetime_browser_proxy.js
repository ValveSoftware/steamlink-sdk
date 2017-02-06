// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function LifetimeBrowserProxy() {}

  LifetimeBrowserProxy.prototype = {
    // Triggers a browser restart.
    restart: function() {},

    // Triggers a browser relaunch.
    relaunch: function() {},

<if expr="chromeos">
    // First signs out current user and then performs a restart.
    logOutAndRestart: function() {},

    // Triggers a factory reset.
    factoryReset: function() {},
</if>
  };

  /**
   * @constructor
   * @implements {settings.LifetimeBrowserProxy}
   */
  function LifetimeBrowserProxyImpl() {}
  cr.addSingletonGetter(LifetimeBrowserProxyImpl);

  LifetimeBrowserProxyImpl.prototype = {
    /** @override */
    restart: function() {
      chrome.send('restart');
    },

    /** @override */
    relaunch: function() {
      chrome.send('relaunch');
    },

<if expr="chromeos">
    /** @override */
    logOutAndRestart: function() {
      chrome.send('logOutAndRestart');
    },

    /** @override */
    factoryReset: function() {
      chrome.send('factoryReset');
    },
</if>
  };

  return {
    LifetimeBrowserProxy: LifetimeBrowserProxy,
    LifetimeBrowserProxyImpl: LifetimeBrowserProxyImpl,
  };
});
