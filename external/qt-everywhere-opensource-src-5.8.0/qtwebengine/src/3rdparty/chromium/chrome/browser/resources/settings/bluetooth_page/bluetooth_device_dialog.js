// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

(function() {

var PairingEventType = chrome.bluetoothPrivate.PairingEventType;

// NOTE(dbeam): even though these behaviors are only used privately, they must
// be globally accessible for Closure's --polymer_pass to compile happily.

/** @polymerBehavior */
settings.BluetoothAddDeviceBehavior = {
  properties: {
    /**
     * The cached bluetooth adapter state.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: {
      type: Object,
      observer: 'adapterStateChanged_',
    },

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    deviceList: {
      type: Array,
      value: /** @return {Array} */ function() { return []; },
    },
  },

  observers: ['deviceListChanged_(deviceList.*)'],

  /** @private */
  adapterStateChanged_: function() {
    if (!this.adapterState.powered)
      this.fire('close-dialog');
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {boolean}
   * @private
   */
  deviceNotPaired_: function(device) {
    return !device.paired;
  },

  /**
   * @param {!Array<!chrome.bluetooth.Device>} deviceList
   * @return {boolean} True if deviceList contains any unpaired devices.
   * @private
   */
  haveDevices_: function(deviceList) {
    return this.deviceList.findIndex(function(d) { return !d.paired; }) != -1;
  },

  /**
   * @param {!{detail: {action: string, device: !chrome.bluetooth.Device}}} e
   * @private
   */
  onDeviceEvent_: function(e) {
    this.fire('device-event', e.detail);
    /** @type {Event} */(e).stopPropagation();
  },
};

/** @polymerBehavior */
settings.BluetoothPairDeviceBehavior = {
  properties: {
    /**
     * Current Pairing device.
     * @type {?chrome.bluetooth.Device|undefined}
     */
    pairingDevice: Object,

    /**
     * Current Pairing event.
     * @type {?chrome.bluetoothPrivate.PairingEvent|undefined}
     */
    pairingEvent: Object,

    /**
     * @const
     * @type {!Array<number>}
     */
    digits: {
      type: Array,
      readOnly: true,
      value: [0, 1, 2, 3, 4, 5],
    },
  },

  observers: [
    'pairingChanged_(pairingDevice, pairingEvent)',
  ],

  /**
   * @param {?chrome.bluetooth.Device} pairingDevice
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @private
   */
  pairingChanged_: function(pairingDevice, pairingEvent) {
    // Auto-close the dialog when pairing completes.
    if (pairingDevice && pairingDevice.connected) {
      this.fire('close-dialog', '');
      return;
    }
  },

  /**
   * @param {?chrome.bluetooth.Device} device
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {string}
   * @private
   */
  getMessage_: function(device, pairingEvent) {
    if (!device)
      return '';
    var message;
    if (!pairingEvent)
      message = 'bluetoothStartConnecting';
    else
      message = this.getEventDesc_(pairingEvent.pairing);
    return this.i18n(message, device.name);
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showEnterPincode_: function(pairingEvent) {
    return !!pairingEvent &&
        pairingEvent.pairing == PairingEventType.REQUEST_PINCODE;
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showEnterPasskey_: function(pairingEvent) {
    return !!pairingEvent &&
        pairingEvent.pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showDisplayPassOrPin_: function(pairingEvent) {
    if (!pairingEvent)
      return false;
    var pairing = pairingEvent.pairing;
    return (
        pairing == PairingEventType.DISPLAY_PINCODE ||
        pairing == PairingEventType.DISPLAY_PASSKEY ||
        pairing == PairingEventType.CONFIRM_PASSKEY ||
        pairing == PairingEventType.KEYS_ENTERED);
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showAcceptReject_: function(pairingEvent) {
    return !!pairingEvent &&
        pairingEvent.pairing == PairingEventType.CONFIRM_PASSKEY;
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showConnect_: function(pairingEvent) {
    if (!pairingEvent)
      return false;
    var pairing = pairingEvent.pairing;
    if (pairing == PairingEventType.REQUEST_PINCODE) {
      var pincode = /** @type {{invalid: boolean}} */(this.$.pincode);
      return !pincode.invalid;
    } else if (pairing == PairingEventType.REQUEST_PASSKEY) {
      var passkey = /** @type {{invalid: boolean}} */(this.$.passkey);
      return !passkey.invalid;
    }
    return false;
  },

  /**
   * @param {?chrome.bluetooth.Device} device
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @return {boolean}
   * @private
   */
  showDismiss_: function(device, pairingEvent) {
    return (!!device && device.paired) ||
        (!!pairingEvent && pairingEvent.pairing == PairingEventType.COMPLETE);
  },

  /** @private */
  onAcceptTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onConnectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CONFIRM);
  },

  /** @private */
  onRejectTap_: function() {
    this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.REJECT);
  },

  /** @private */
  onDismissTap_: function() { this.fire('close-dialog', ''); },

  /** @private */
  sendResponse_: function(response) {
    if (!this.pairingDevice)
      return;
    var options =
        /** @type {!chrome.bluetoothPrivate.SetPairingResponseOptions} */ {
          device: this.pairingDevice,
          response: response
        };
    if (response == chrome.bluetoothPrivate.PairingResponse.CONFIRM) {
      var pairing = this.pairingEvent.pairing;
      if (pairing == PairingEventType.REQUEST_PINCODE)
        options.pincode = this.$.pincode.value;
      else if (pairing == PairingEventType.REQUEST_PASSKEY)
        options.passkey = parseInt(this.$.passkey.value, 10);
    }
    this.fire('response', options);
  },

  /**
   * @param {!PairingEventType} eventType
   * @return {string}
   * @private
   */
  getEventDesc_: function(eventType) {
    assert(eventType);
    if (eventType == PairingEventType.COMPLETE ||
        eventType == PairingEventType.KEYS_ENTERED ||
        eventType == PairingEventType.REQUEST_AUTHORIZATION) {
      return 'bluetoothStartConnecting';
    }
    return 'bluetooth_' + /** @type {string} */(eventType);
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinDigit_: function(pairingEvent, index) {
    if (!pairingEvent)
      return '';
    var digit = '0';
    var pairing = pairingEvent.pairing;
    if (pairing == PairingEventType.DISPLAY_PINCODE && pairingEvent.pincode &&
        index < pairingEvent.pincode.length) {
      digit = pairingEvent.pincode[index];
    } else if (pairingEvent.passkey &&
               (pairing == PairingEventType.DISPLAY_PASSKEY ||
                pairing == PairingEventType.KEYS_ENTERED ||
                pairing == PairingEventType.CONFIRM_PASSKEY)) {
      var passkeyString = String(pairingEvent.passkey);
      if (index < passkeyString.length)
        digit = passkeyString[index];
    }
    return digit;
  },

  /**
   * @param {?chrome.bluetoothPrivate.PairingEvent} pairingEvent
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinClass_: function(pairingEvent, index) {
    if (!pairingEvent)
      return '';
    if (pairingEvent.pairing == PairingEventType.CONFIRM_PASSKEY)
      return 'confirm';
    var cssClass = 'display';
    if (pairingEvent.pairing == PairingEventType.DISPLAY_PASSKEY) {
      if (index == 0)
        cssClass += ' next';
      else
        cssClass += ' untyped';
    } else if (
        pairingEvent.pairing == PairingEventType.KEYS_ENTERED &&
        pairingEvent.enteredKey) {
      var enteredKey = pairingEvent.enteredKey;  // 1-7
      var lastKey = this.digits.length;  // 6
      if ((index == -1 && enteredKey > lastKey) || (index + 1 == enteredKey))
        cssClass += ' next';
      else if (index > enteredKey)
        cssClass += ' untyped';
    }
    return cssClass;
  },
};

Polymer({
  is: 'bluetooth-device-dialog',

  behaviors: [
    I18nBehavior,
    settings.BluetoothAddDeviceBehavior,
    settings.BluetoothPairDeviceBehavior,
  ],

  properties: {
    /** Which version of this dialog to show (adding or pairing). */
    dialogType: String,
  },

  /** @private */
  deviceListChanged_: function(e) {
    this.$.dialog.notifyResize();
  },

  /**
   * @param {string} dialogType
   * @return {string} The title of for that |dialogType|.
   */
  getTitle_: function(dialogType) {
    return this.i18n(dialogType == 'addDevice' ?
        'bluetoothAddDevicePageTitle' : 'bluetoothPairDevicePageTitle');
  },

  /**
   * @param {string} currentDialogType
   * @param {string} wantedDialogType
   * @return {boolean}
   * @private
   */
  isDialogType_: function(currentDialogType, wantedDialogType) {
    return currentDialogType == wantedDialogType;
  },

  /** @private */
  onCancelTap_: function() {
    // NOTE: tapping on an element with [dialog-dismiss] doesn't trigger an
    // iron-overlay-cancel event, whereas tapping (X) or pressing Esc does.
    // Using cancel() ensures all 3 ways to close the dialog run the same code.
    this.$.dialog.cancel();
  },

  /** @private */
  onIronOverlayCanceled_: function() {
    if (this.dialogType == 'pairDevice')
      this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CANCEL);
  },

  /** @private */
  onIronOverlayClosed_: function() {
    this.fire('close-dialog', '');
  },

  open: function() {
    this.$.dialog.open();
  },

  close: function() {
    this.$.dialog.close();
  },
});

})();
