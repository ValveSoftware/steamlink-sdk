// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a bluetooth device in a list.
 */

Polymer({
  is: 'bluetooth-device-list-item',

  behaviors: [I18nBehavior],

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
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_: function(event) {
    let button = /** @type {!HTMLElement} */ (event.target);
    let menu = /** @type {!CrActionMenuElement} */ (this.$.dotsMenu);
    menu.showAt(button);
    event.stopPropagation();
  },

  /** @private */
  onConnectActionTap_: function() {
    let action = this.isDisconnected_(this.device) ? 'connect' : 'disconnect';
    this.fire('device-event', {
      action: action,
      device: this.device,
    });
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /** @private */
  onRemoveTap_: function() {
    this.fire('device-event', {
      action: 'remove',
      device: this.device,
    });
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
  },

  /**
   * @param {boolean} connected
   * @return {string} The text to display for the connect/disconnect menu item.
   * @private
   */
  getConnectActionText_: function(connected) {
    return this.i18n(connected ? 'bluetoothDisconnect' : 'bluetoothConnect');
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
