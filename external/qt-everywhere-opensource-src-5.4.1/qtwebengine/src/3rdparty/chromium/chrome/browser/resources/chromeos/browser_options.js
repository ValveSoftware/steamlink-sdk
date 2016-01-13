// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the BrowserOptions calls from
   * BluetoothOptionsHandler that is registered by the webUI,
   * ie, BluetoothPairingUI.
   * @constructor
   */
  function BrowserOptions() {
    OptionsPage.call(this,
                     'bluetooth',
                     '',
                     'bluetooth-container');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
    },
  };

  BrowserOptions.showBluetoothSettings = function() {
  };

  BrowserOptions.setBluetoothState = function() {
  };

  /**
   * Handles addBluetoothDevice call, display the Bluetooth pairing overlay
   * for the pairing device.
   * @param {{name: string,
   *          address: string,
   *          paired: boolean,
   *          pairing: string | undefined
   *          pincode: string | undefined
   *          passkey: number | undefined
   *          connected: boolean}} device
   */
  BrowserOptions.addBluetoothDevice = function(device) {
    // One device can be in the process of pairing.  If found, display
    // the Bluetooth pairing overlay.
    if (device.pairing)
      BluetoothPairing.showDialog(device);
  };

  BrowserOptions.removeBluetoothDevice = function(address) {
  };

  // Export
  return {
    BrowserOptions: BrowserOptions
  };
});
