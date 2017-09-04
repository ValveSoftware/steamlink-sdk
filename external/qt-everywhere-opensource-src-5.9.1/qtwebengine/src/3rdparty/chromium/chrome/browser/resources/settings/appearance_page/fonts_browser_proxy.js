// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   fontList: Array<{0: string, 1: (string|undefined), 2: (string|undefined)}>,
 *   extensionUrl: string
 * }}
 */
var FontsData;

cr.define('settings', function() {
  /** @interface */
  function FontsBrowserProxy() {}

  FontsBrowserProxy.prototype = {
    /**
     * @return {!Promise<!FontsData>} Fonts and the advanced font settings
     *     extension URL.
     */
    fetchFontsData: assertNotReached,

    observeAdvancedFontExtensionAvailable: assertNotReached,

    openAdvancedFontSettings: assertNotReached,
  };

  /**
   * @implements {settings.FontsBrowserProxy}
   * @constructor
   */
  function FontsBrowserProxyImpl() {}

  cr.addSingletonGetter(FontsBrowserProxyImpl);

  FontsBrowserProxyImpl.prototype = {
    /** @override */
    fetchFontsData: function() {
      return cr.sendWithPromise('fetchFontsData');
    },

    /** @override */
    observeAdvancedFontExtensionAvailable: function() {
      chrome.send('observeAdvancedFontExtensionAvailable');
    },

    /** @override */
    openAdvancedFontSettings: function() {
      chrome.send('openAdvancedFontSettings');
    }
  };

  return {
    FontsBrowserProxy: FontsBrowserProxy,
    FontsBrowserProxyImpl: FontsBrowserProxyImpl,
  };
});
