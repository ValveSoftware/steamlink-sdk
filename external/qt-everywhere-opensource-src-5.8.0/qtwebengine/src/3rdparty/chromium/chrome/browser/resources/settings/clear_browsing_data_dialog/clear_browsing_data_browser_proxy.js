// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Clear browsing data" dialog
 * to interact with the browser.
 */

cr.define('settings', function() {
  /** @interface */
  function ClearBrowsingDataBrowserProxy() {}

  ClearBrowsingDataBrowserProxy.prototype = {
    /**
     * @return {!Promise} A promise resolved when data clearing has completed.
     */
    clearBrowsingData: function() {},

    /** Initializes the dialog. */
    initialize: function() {},
  };

  /**
   * @constructor
   * @implements {settings.ClearBrowsingDataBrowserProxy}
   */
  function ClearBrowsingDataBrowserProxyImpl() {}
  cr.addSingletonGetter(ClearBrowsingDataBrowserProxyImpl);

  ClearBrowsingDataBrowserProxyImpl.prototype = {
    /** @override */
    clearBrowsingData: function() {
      return cr.sendWithPromise('clearBrowsingData');
    },

    /** @override */
    initialize: function() {
      chrome.send('initializeClearBrowsingData');
    },
  };

  return {
    ClearBrowsingDataBrowserProxy: ClearBrowsingDataBrowserProxy,
    ClearBrowsingDataBrowserProxyImpl: ClearBrowsingDataBrowserProxyImpl,
  };
});
