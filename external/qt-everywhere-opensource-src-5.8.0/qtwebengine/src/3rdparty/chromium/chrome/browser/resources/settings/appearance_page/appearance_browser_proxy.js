// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function AppearanceBrowserProxy() {}

  AppearanceBrowserProxy.prototype = {
    /**
     * @return {!Promise<boolean>} Whether the theme may be reset.
     */
    getResetThemeEnabled: assertNotReached,

<if expr="chromeos">
    openWallpaperManager: assertNotReached,
</if>

    resetTheme: assertNotReached,
  };

  /**
   * @implements {settings.AppearanceBrowserProxy}
   * @constructor
   */
  function AppearanceBrowserProxyImpl() {}

  cr.addSingletonGetter(AppearanceBrowserProxyImpl);

  AppearanceBrowserProxyImpl.prototype = {
    /** @override */
    getResetThemeEnabled: function() {
      return cr.sendWithPromise('getResetThemeEnabled');
    },

<if expr="chromeos">
    /** @override */
    openWallpaperManager: function() {
      chrome.send('openWallpaperManager');
    },
</if>

    /** @override */
    resetTheme: function() {
      chrome.send('resetTheme');
    },
  };

  return {
    AppearanceBrowserProxy: AppearanceBrowserProxy,
    AppearanceBrowserProxyImpl: AppearanceBrowserProxyImpl,
  };
});
