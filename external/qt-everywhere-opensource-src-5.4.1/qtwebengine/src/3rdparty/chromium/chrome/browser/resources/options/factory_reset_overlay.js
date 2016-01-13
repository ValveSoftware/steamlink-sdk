// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * FactoryResetOverlay class
   * Encapsulated handling of the Factory Reset confirmation overlay page.
   * @class
   */
  function FactoryResetOverlay() {
    OptionsPage.call(this, 'factoryResetData',
                     loadTimeData.getString('factoryResetTitle'),
                     'factory-reset-overlay');
  }

  cr.addSingletonGetter(FactoryResetOverlay);

  FactoryResetOverlay.prototype = {
    // Inherit FactoryResetOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      $('factory-reset-data-dismiss').onclick = function(event) {
        FactoryResetOverlay.dismiss();
      };
      $('factory-reset-data-restart').onclick = function(event) {
        chrome.send('performFactoryResetRestart');
      };
    },
  };

  FactoryResetOverlay.dismiss = function() {
    OptionsPage.closeOverlay();
  };

  // Export
  return {
    FactoryResetOverlay: FactoryResetOverlay
  };
});
