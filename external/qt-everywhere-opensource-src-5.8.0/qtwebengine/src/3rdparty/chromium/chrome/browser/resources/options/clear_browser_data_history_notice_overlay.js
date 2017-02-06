// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A popup that may be shown on top of the Clear Browsing Data
 *     overlay after the deletion finished.
 */

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * A notice to be shown atop of the Clear Browsing Data overlay after
   * the deletion of browsing history, informing the user that other forms
   * of browsing history are still available.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ClearBrowserDataHistoryNotice() {
    Page.call(this, 'clearBrowserDataHistoryNotice',
                    loadTimeData.getString('clearBrowserDataOverlayTabTitle'),
                    'clear-browser-data-history-notice');
  }

  cr.addSingletonGetter(ClearBrowserDataHistoryNotice);

  ClearBrowserDataHistoryNotice.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      $('clear-browser-data-history-notice-ok').onclick = function() {
        // Close this popup, and the Clear Browsing Data overlay below it.
        PageManager.closeOverlay();
        ClearBrowserDataOverlay.dismiss();
      };
    },
  };

  return {
    ClearBrowserDataHistoryNotice: ClearBrowserDataHistoryNotice
  };
});
