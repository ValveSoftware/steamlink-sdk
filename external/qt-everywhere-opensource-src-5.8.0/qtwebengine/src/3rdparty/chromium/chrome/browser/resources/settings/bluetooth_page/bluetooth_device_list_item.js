// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a bluetooth device in a list.
 */

Polymer({
  is: 'bluetooth-device-list-item',

  properties: {
    /**
     * The bluetooth device.
     * @type {!chrome.bluetooth.Device}
     */
    device: {
      type: Object,
    },
  },

  /**
   * @param {Event} e
   * @private
   */
  itemTapped_: function(e) {
    this.fire('device-event', {
      action: 'connect',
      device: this.device,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  menuSelected_: function(e) {
    e.currentTarget.opened = false;
    this.fire('device-event', {
      action: e.target.id,
      device: this.device,
    });
  },

  /**
   * @param {Event} e
   * @private
   */
  doNothing_: function(e) {
    // Avoid triggering itemTapped_.
    e.stopPropagation();
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display for |device| in the device list.
   * @private
   */
  getDeviceName_: function(device) {
    return device.name || device.address;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean}
   * @private
   */
  isDisconnected_: function(device) {
    return !device.connected && !device.connecting;
  },
});
