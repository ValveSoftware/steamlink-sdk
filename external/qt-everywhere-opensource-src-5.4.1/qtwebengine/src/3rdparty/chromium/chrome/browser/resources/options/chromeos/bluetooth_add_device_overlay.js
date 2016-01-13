// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the Bluetooth options page.
   * @constructor
   */
  function BluetoothOptions() {
    OptionsPage.call(this,
                     'bluetooth',
                     loadTimeData.getString('bluetoothOptionsPageTabTitle'),
                     'bluetooth-options');
  }

  cr.addSingletonGetter(BluetoothOptions);

  BluetoothOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The list of available (unpaired) bluetooth devices.
     * @type {DeletableItemList}
     * @private
     */
    deviceList_: null,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      this.createDeviceList_();

      BluetoothOptions.updateDiscoveryState(true);

      $('bluetooth-add-device-cancel-button').onclick = function(event) {
        OptionsPage.closeOverlay();
      };

      var self = this;
      $('bluetooth-add-device-apply-button').onclick = function(event) {
        var device = self.deviceList_.selectedItem;
        var address = device.address;
        OptionsPage.closeOverlay();
        device.pairing = 'bluetoothStartConnecting';
        options.BluetoothPairing.showDialog(device);
        chrome.send('updateBluetoothDevice', [address, 'connect']);
      };

      $('bluetooth-unpaired-devices-list').addEventListener('change',
                                                            function() {
        var item = $('bluetooth-unpaired-devices-list').selectedItem;
        // The "bluetooth-add-device-apply-button" should be enabled for devices
        // that can be paired or remembered. Devices not supporting pairing will
        // be just remembered and later reported as "item.paired" = true. The
        // button should be disabled in any other case:
        // * No item is selected (item is undefined).
        // * Paired devices (item.paired is true) are already paired and a new
        //   pairing attempt will fail. Paired devices could appear in this list
        //   shortly after the pairing initiated in another window finishes.
        // * "Connecting" devices (item.connecting is true) are in the process
        //   of a pairing or connection. Another attempt to pair before the
        //   ongoing pair finishes will fail, so the button should be disabled.
        var disabled = !item || item.paired || item.connecting;
        $('bluetooth-add-device-apply-button').disabled = disabled;
      });
    },

    /** @override */
    didClosePage: function() {
      chrome.send('stopBluetoothDeviceDiscovery');
    },

    /**
     * Creates, decorates and initializes the bluetooth device list.
     * @private
     */
    createDeviceList_: function() {
      this.deviceList_ = $('bluetooth-unpaired-devices-list');
      options.system.bluetooth.BluetoothDeviceList.decorate(this.deviceList_);
    }
  };

  /**
   * Automatically start the device discovery process if the
   * "Add device" dialog is visible.
   */
  BluetoothOptions.startDeviceDiscovery = function() {
    var page = BluetoothOptions.getInstance();
    if (page && page.visible)
      chrome.send('findBluetoothDevices');
  };

  /**
   * Updates the dialog to show that device discovery has stopped. Updates the
   * label text and hides/unhides the spinner. based on discovery state.
   */
  BluetoothOptions.updateDiscoveryState = function(discovering) {
    $('bluetooth-scanning-label').hidden = !discovering;
    $('bluetooth-scanning-icon').hidden = !discovering;
    $('bluetooth-scan-stopped-label').hidden = discovering;
  };

  /**
   * If the "Add device" dialog is visible, dismiss it.
   */
  BluetoothOptions.dismissOverlay = function() {
    var page = BluetoothOptions.getInstance();
    if (page && page.visible)
      OptionsPage.closeOverlay();
  };

  // Export
  return {
    BluetoothOptions: BluetoothOptions
  };
});
