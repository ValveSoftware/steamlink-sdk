// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function AppearanceBrowserProxy() {}

  AppearanceBrowserProxy.prototype = {
    /** @return {!Promise<number>} */
    getDefaultZoom: assertNotReached,

    /**
     * @param {string} themeId
     * @return {!Promise<!chrome.management.ExtensionInfo>} Theme info.
     */
    getThemeInfo: assertNotReached,

    /** @return {boolean} Whether the current profile is supervised. */
    isSupervised: assertNotReached,

<if expr="chromeos">
    openWallpaperManager: assertNotReached,
</if>

    useDefaultTheme: assertNotReached,

<if expr="is_linux and not chromeos">
    useSystemTheme: assertNotReached,
</if>
  };

  /**
   * @implements {settings.AppearanceBrowserProxy}
   * @constructor
   */
  function AppearanceBrowserProxyImpl() {}

  cr.addSingletonGetter(AppearanceBrowserProxyImpl);

  AppearanceBrowserProxyImpl.prototype = {
    /** @override */
    getDefaultZoom: function() {
      return new Promise(function(resolve) {
        chrome.settingsPrivate.getDefaultZoom(resolve);
      });
    },

    /** @override */
    getThemeInfo: function(themeId) {
      return new Promise(function(resolve) {
        chrome.management.get(themeId, resolve);
      });
    },

    /** @override */
    isSupervised: function() {
      return loadTimeData.getBoolean('isSupervised');
    },

<if expr="chromeos">
    /** @override */
    openWallpaperManager: function() {
      chrome.send('openWallpaperManager');
    },
</if>

    /** @override */
    useDefaultTheme: function() {
      chrome.send('useDefaultTheme');
    },

<if expr="is_linux and not chromeos">
    /** @override */
    useSystemTheme: function() {
      chrome.send('useSystemTheme');
    },
</if>
  };

  return {
    AppearanceBrowserProxy: AppearanceBrowserProxy,
    AppearanceBrowserProxyImpl: AppearanceBrowserProxyImpl,
  };
});
