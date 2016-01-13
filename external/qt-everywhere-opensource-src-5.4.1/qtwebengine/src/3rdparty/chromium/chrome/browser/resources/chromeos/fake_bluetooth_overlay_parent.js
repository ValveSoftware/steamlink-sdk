// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated a fake parent page for bluetooth overlay page used by Web UI.
   * @constructor
   */
  function FakeBluetoothOverlayParent(model) {
    OptionsPage.call(this, 'bluetooth',
                     '',
                     'bluetooth-container');
  }

  cr.addSingletonGetter(FakeBluetoothOverlayParent);

  FakeBluetoothOverlayParent.prototype = {
    // Inherit FakeBluetoothOverlayParent from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes FakeBluetoothOverlayParent page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);
    },
  };

  // Export
  return {
    FakeBluetoothOverlayParent: FakeBluetoothOverlayParent
  };
});
