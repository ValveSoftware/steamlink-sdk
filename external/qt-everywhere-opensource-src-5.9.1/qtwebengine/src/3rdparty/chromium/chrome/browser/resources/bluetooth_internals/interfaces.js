// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Mojo interface helpers, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('interfaces', function() {
  /**
   * Sets up Mojo interfaces and adds them to window.interfaces.
   * @return {Promise}
   */
  function setupInterfaces() {
    return importModules([
      'content/public/renderer/frame_interfaces',
      'device/bluetooth/public/interfaces/adapter.mojom',
      'device/bluetooth/public/interfaces/device.mojom',
      'mojo/public/js/connection',
    ]).then(function([frameInterfaces, bluetoothAdapter, bluetoothDevice,
        connection]) {
      interfaces.BluetoothAdapter = bluetoothAdapter;
      interfaces.BluetoothDevice = bluetoothDevice;
      interfaces.Connection = connection;
      interfaces.FrameInterfaces = frameInterfaces;
    });
  }

  return {
    setupInterfaces: setupInterfaces,
  };
});
