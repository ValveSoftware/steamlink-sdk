// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for AdapterBroker, served from
 *     chrome://bluetooth-internals/.
 */
cr.define('adapter_broker', function() {
  /**
   * The proxy class of an adapter and router of adapter events.
   * Exposes an EventTarget interface that allows other object to subscribe to
   * to specific AdapterClient events.
   * Provides proxy access to Adapter functions. Converts parameters to Mojo
   * handles and back when necessary.
   * @constructor
   * @extends {cr.EventTarget}
   * @param {!interfaces.BluetoothAdapter.Adapter.proxyClass} adapter
   */
  var AdapterBroker = function(adapter) {
    this.adapter_ = adapter;
    this.adapterClient_ = new AdapterClient(this);
    this.setClient(this.adapterClient_);
  };

  AdapterBroker.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Sets client of Adapter service.
     * @param {!interfaces.BluetoothAdapter.AdapterClient} adapterClient
     */
    setClient: function(adapterClient) {
      this.adapter_.setClient(interfaces.Connection.bindStubDerivedImpl(
          adapterClient));
    },

    /**
     * Gets an array of currently detectable devices from the Adapter service.
     * @return {!Array<!interfaces.BluetoothDevice.DeviceInfo>}
     */
    getDevices: function() {
      return this.adapter_.getDevices();
    },

    /**
     * Gets the current state of the Adapter.
     * @return {!interfaces.BluetoothAdapter.AdapterInfo}
     */
    getInfo: function() {
      return this.adapter_.getInfo();
    }
  };

  /**
   * The implementation of AdapterClient in
   * device/bluetooth/public/interfaces/adapter.mojom. Dispatches events
   * through AdapterBroker to notify client objects of changes to the Adapter
   * service.
   * @constructor
   * @param {!AdapterBroker} adapterBroker Broker to dispatch events through.
   */
  var AdapterClient = function(adapterBroker) {
    this.adapterBroker_ = adapterBroker;
  };

  AdapterClient.prototype = {
    /**
     * Fires deviceadded event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceAdded: function(deviceInfo) {
      var event = new CustomEvent('deviceadded', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },

    /**
     * Fires deviceremoved event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceRemoved: function(deviceInfo) {
      var event = new CustomEvent('deviceremoved', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    },

    /**
     * Fires devicechanged event.
     * @param {!interfaces.BluetoothDevice.DeviceInfo} deviceInfo
     */
    deviceChanged: function(deviceInfo) {
      var event = new CustomEvent('devicechanged', {
        detail: {
          deviceInfo: deviceInfo
        }
      });
      this.adapterBroker_.dispatchEvent(event);
    }
  };

  var adapterBroker = null;

  /**
   * Initializes an AdapterBroker if one doesn't exist.
   * @return {!Promise<!AdapterBroker>} resolves with AdapterBroker,
   *     rejects if Bluetooth is not supported.
   */
  function getAdapterBroker() {
    if (adapterBroker) return Promise.resolve(adapterBroker);

    return interfaces.setupInterfaces().then(function(adapter) {
      // Hook up the instance properties.
      AdapterClient.prototype.__proto__ =
          interfaces.BluetoothAdapter.AdapterClient.stubClass.prototype;

      var adapterFactory = interfaces.Connection.bindHandleToProxy(
          interfaces.FrameInterfaces.getInterface(
              interfaces.BluetoothAdapter.AdapterFactory.name),
          interfaces.BluetoothAdapter.AdapterFactory);

      // Get an Adapter service.
      return adapterFactory.getAdapter();
    }).then(function(response) {
      if (!response.adapter) {
        throw new Error('Bluetooth Not Supported on this platform.');
      }

      var adapter = interfaces.Connection.bindHandleToProxy(
          response.adapter,
          interfaces.BluetoothAdapter.Adapter);

      adapterBroker = new AdapterBroker(adapter);
      return adapterBroker;
    });
  }

  return {
    getAdapterBroker: getAdapterBroker,
  };
});
