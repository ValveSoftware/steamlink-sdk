// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DeviceCollection, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('device_collection', function() {
  /*
   * Collection of devices. Extends ArrayDataModel which provides a set of
   * functions and events that notifies observers when the collection changes.
   * @constructor
   * @param {!Array<device_collection.Device>} array The starting collection of
   *     devices.
   * @extends {cr.ui.ArrayDataModel}
   */
  var DeviceCollection = function(array) {
    cr.ui.ArrayDataModel.call(this, array);
  };
  DeviceCollection.prototype = {
    __proto__: cr.ui.ArrayDataModel.prototype,

    /**
     * Finds the Device in the collection with the matching address.
     * @param {string} address
     */
    getByAddress: function(address) {
      for (var i = 0; i < this.length; i++) {
        var device = this.item(i);
        if (address == device.info.address)
          return device;
      }
      return null;
    },

    /**
     * Adds or updates a Device with new DeviceInfo.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    addOrUpdate: function(deviceInfo) {
      var oldDevice = this.getByAddress(deviceInfo.address);
      if (oldDevice) {
        // Update rssi if it's valid
        var rssi = (deviceInfo.rssi && deviceInfo.rssi.value) ||
            (oldDevice.info.rssi && oldDevice.info.rssi.value);

        oldDevice.info = deviceInfo;
        oldDevice.info.rssi = { value: rssi };
        oldDevice.removed = false;

        this.updateIndex(this.indexOf(oldDevice));
      } else {
        this.push(new Device(deviceInfo));
      }
    },

    /**
     * Marks the Device as removed.
     * @param {!interfaces.bluetoothDevice.DeviceInfo} deviceInfo
     */
    remove: function(deviceInfo) {
      var device = this.getByAddress(deviceInfo.address);
      assert(device, 'Device does not exist.');
      device.removed = true;
      this.updateIndex(this.indexOf(device));
    }
  };

  /*
   * Data model for a cached device.
   * @constructor
   * @param {!interfaces.BluetoothDevice.DeviceInfo} info
   */
  var Device = function(info) {
    this.info = info;
    this.removed = false;
  };

  return {
    Device: Device,
    DeviceCollection: DeviceCollection,
  };
});
