// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for bluetooth_internals.html, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('bluetooth_internals', function() {
  function initializeViews() {
    var adapterBroker = null;
    adapter_broker.getAdapterBroker()
      .then(function(broker) { adapterBroker = broker; })
      .then(function() { return adapterBroker.getInfo(); })
      .then(function(response) { console.log('adapter', response.info); })
      .then(function() { return adapterBroker.getDevices(); })
      .then(function(response) {
        // Hook up device collection events.
        var devices = new device_collection.DeviceCollection([]);
        adapterBroker.addEventListener('deviceadded', function(event) {
          devices.addOrUpdate(event.detail.deviceInfo);
        });
        adapterBroker.addEventListener('devicechanged', function(event) {
          devices.addOrUpdate(event.detail.deviceInfo);
        });
        adapterBroker.addEventListener('deviceremoved', function(event) {
          devices.remove(event.detail.deviceInfo);
        });

        response.devices.forEach(devices.addOrUpdate,
                                 devices /* this */);

        var deviceTable = new device_table.DeviceTable();
        deviceTable.setDevices(devices);
        document.body.appendChild(deviceTable);
      })
      .catch(function(error) { console.error(error); });
  }

  return {
    initializeViews: initializeViews
  };

});

document.addEventListener(
    'DOMContentLoaded', bluetooth_internals.initializeViews);
