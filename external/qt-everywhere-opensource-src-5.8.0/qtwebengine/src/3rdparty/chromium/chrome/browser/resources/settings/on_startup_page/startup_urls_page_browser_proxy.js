// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   modelIndex: number,
 *   title: string,
 *   tooltip: string,
 *   url: string
 * }}
 */
var StartupPageInfo;

cr.define('settings', function() {
  /** @interface */
  function StartupUrlsPageBrowserProxy() {}

  StartupUrlsPageBrowserProxy.prototype = {
    loadStartupPages: assertNotReached,

    useCurrentPages: assertNotReached,

    /**
     * @param {string} url
     * @return {!Promise<boolean>} Whether the URL is valid.
     */
    validateStartupPage: assertNotReached,

    /**
     * @param {string} url
     * @return {!Promise<boolean>} Whether the URL was actually added, or
     *     ignored because it was invalid.
     */
    addStartupPage: assertNotReached,

    /**
     * @param {number} modelIndex
     * @param {string} url
     * @return {!Promise<boolean>} Whether the URL was actually edited, or
     *     ignored because it was invalid.
     */
    editStartupPage: assertNotReached,

    /** @param {number} index */
    removeStartupPage: assertNotReached,
  };

  /**
   * @implements {settings.StartupUrlsPageBrowserProxy}
   * @constructor
   */
  function StartupUrlsPageBrowserProxyImpl() {}

  cr.addSingletonGetter(StartupUrlsPageBrowserProxyImpl);

  StartupUrlsPageBrowserProxyImpl.prototype = {
    /** @override */
    loadStartupPages: function() {
      chrome.send('onStartupPrefsPageLoad');
    },

    /** @override */
    useCurrentPages: function() {
      chrome.send('setStartupPagesToCurrentPages');
    },

    /** @override */
    validateStartupPage: function(url) {
      return cr.sendWithPromise('validateStartupPage', url);
    },

    /** @override */
    addStartupPage: function(url) {
      return cr.sendWithPromise('addStartupPage', url);
    },

    /** @override */
    editStartupPage: function(modelIndex, url) {
      return cr.sendWithPromise('editStartupPage', modelIndex, url);
    },

    /** @override */
    removeStartupPage: function(index) {
      chrome.send('removeStartupPage', [index]);
    },
  };

  return {
    StartupUrlsPageBrowserProxy: StartupUrlsPageBrowserProxy,
    StartupUrlsPageBrowserProxyImpl: StartupUrlsPageBrowserProxyImpl,
  };
});
