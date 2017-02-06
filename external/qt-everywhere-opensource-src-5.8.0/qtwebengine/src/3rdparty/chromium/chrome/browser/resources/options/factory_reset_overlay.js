// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * FactoryResetOverlay class
   * Encapsulated handling of the Factory Reset confirmation overlay page.
   * @class
   */
  function FactoryResetOverlay() {
    Page.call(this, 'factoryResetData',
              loadTimeData.getString('factoryResetTitle'),
              'factory-reset-overlay');
  }

  cr.addSingletonGetter(FactoryResetOverlay);

  FactoryResetOverlay.prototype = {
    // Inherit FactoryResetOverlay from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('factory-reset-data-dismiss').onclick = function(event) {
        FactoryResetOverlay.dismiss();
      };
      $('factory-reset-data-restart').onclick = function(event) {
        chrome.send('performFactoryResetRestart');
      };
    },
  };

  FactoryResetOverlay.dismiss = function() {
    PageManager.closeOverlay();
  };

  // Export
  return {
    FactoryResetOverlay: FactoryResetOverlay
  };
});
