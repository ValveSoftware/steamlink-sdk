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
      value: /** @return {Array} */ function() {
        return [];
      },
      observer: 'deviceListChanged_',
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     */
    selectedItem: {
      type: Object,
      observer: 'selectedItemChanged_',
    },
  },

  /** @type {boolean} */ itemWasFocused_: false,

  /** @private */
  adapterStateChanged_: function() {
    if (!this.adapterState.powered)
      this.close();
  },

  /** @private */
  deviceListChanged_: function() {
    this.updateScrollableContents();
    if (this.itemWasFocused_ || !this.getUnpaired_().length)
      return;
    // If the iron-list is populated with at least one visible item then
    // focus it.
    let item = this.$$('iron-list bluetooth-device-list-item');
    if (item && item.offsetParent != null) {
      item.focus();
      this.itemWasFocused_ = true;
      return;
    }
    // Otherwise try again.
    setTimeout(function() { this.deviceListChanged_(); }.bind(this), 100);
  },

  /** @private */
  selectedItemChanged_: function() {
    if (this.selectedItem)
      this.fire('device-event', {action: 'connect', device: this.selectedItem});
  },

  /**
   * @return {!Array<!chrome.bluetooth.Device>}
   * @private
   */
  getUnpaired_: function() {
    return this.deviceList.filter(function(device) {
      return !device.paired;
    });
  },

  /**
   * @return {boolean} True if deviceList contains any unpaired devices.
   * @private
   */
  haveUnpaired_: function(deviceList) {
    return this.getUnpaired_().length > 0;
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

    /** Pincode or passkey value, used to trigger connect enabled changes. */
    pinOrPass: String,

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

  /** @private */
  pairingChanged_: function() {
    // Auto-close the dialog when pairing completes.
    if (this.pairingDevice && this.pairingDevice.connected) {
      this.close();
      return;
    }
    this.pinOrPass = '';
  },

  /**
   * @return {string}
   * @private
   */
  getMessage_: function() {
    if (!this.pairingDevice)
      return '';
    var message;
    if (!this.pairingEvent)
      message = 'bluetoothStartConnecting';
    else
      message = this.getEventDesc_(this.pairingEvent.pairing);
    return this.i18n(message, this.pairingDevice.name);
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPincode_: function() {
    return !!this.pairingEvent &&
        this.pairingEvent.pairing == PairingEventType.REQUEST_PINCODE;
  },

  /**
   * @return {boolean}
   * @private
   */
  showEnterPasskey_: function() {
    return !!this.pairingEvent &&
        this.pairingEvent.pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDisplayPassOrPin_: function() {
    if (!this.pairingEvent)
      return false;
    var pairing = this.pairingEvent.pairing;
    return (
        pairing == PairingEventType.DISPLAY_PINCODE ||
        pairing == PairingEventType.DISPLAY_PASSKEY ||
        pairing == PairingEventType.CONFIRM_PASSKEY ||
        pairing == PairingEventType.KEYS_ENTERED);
  },

  /**
   * @return {boolean}
   * @private
   */
  showAcceptReject_: function() {
    return !!this.pairingEvent &&
        this.pairingEvent.pairing == PairingEventType.CONFIRM_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  showConnect_: function() {
    if (!this.pairingEvent)
      return false;
    var pairing = this.pairingEvent.pairing;
    return pairing == PairingEventType.REQUEST_PINCODE ||
        pairing == PairingEventType.REQUEST_PASSKEY;
  },

  /**
   * @return {boolean}
   * @private
   */
  enableConnect_: function() {
    if (!this.showConnect_())
      return false;
    var inputId =
        (this.pairingEvent.pairing == PairingEventType.REQUEST_PINCODE) ?
        '#pincode' :
        '#passkey';
    var paperInput = /** @type {!PaperInputElement} */ (this.$$(inputId));
    assert(paperInput);
    /** @type {string} */ var value = paperInput.value;
    return !!value && paperInput.validate();
  },

  /**
   * @return {boolean}
   * @private
   */
  showDismiss_: function() {
    return (!!this.paringDevice && this.pairingDevice.paired) ||
        (!!this.pairingEvent &&
         this.pairingEvent.pairing == PairingEventType.COMPLETE);
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

  /**
   * @param {!chrome.bluetoothPrivate.PairingResponse} response
   * @private
   */
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
        options.pincode = this.$$('#pincode').value;
      else if (pairing == PairingEventType.REQUEST_PASSKEY)
        options.passkey = parseInt(this.$$('#passkey').value, 10);
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
    return 'bluetooth_' + /** @type {string} */ (eventType);
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinDigit_: function(index) {
    if (!this.pairingEvent)
      return '';
    var digit = '0';
    var pairing = this.pairingEvent.pairing;
    if (pairing == PairingEventType.DISPLAY_PINCODE &&
        this.pairingEvent.pincode && index < this.pairingEvent.pincode.length) {
      digit = this.pairingEvent.pincode[index];
    } else if (
        this.pairingEvent.passkey &&
        (pairing == PairingEventType.DISPLAY_PASSKEY ||
         pairing == PairingEventType.KEYS_ENTERED ||
         pairing == PairingEventType.CONFIRM_PASSKEY)) {
      var passkeyString = String(this.pairingEvent.passkey);
      if (index < passkeyString.length)
        digit = passkeyString[index];
    }
    return digit;
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getPinClass_: function(index) {
    if (!this.pairingEvent)
      return '';
    if (this.pairingEvent.pairing == PairingEventType.CONFIRM_PASSKEY)
      return 'confirm';
    var cssClass = 'display';
    if (this.pairingEvent.pairing == PairingEventType.DISPLAY_PASSKEY) {
      if (index == 0)
        cssClass += ' next';
      else
        cssClass += ' untyped';
    } else if (
        this.pairingEvent.pairing == PairingEventType.KEYS_ENTERED &&
        this.pairingEvent.enteredKey) {
      var enteredKey = this.pairingEvent.enteredKey;  // 1-7
      var lastKey = this.digits.length;               // 6
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
    CrScrollableBehavior,
    settings.BluetoothAddDeviceBehavior,
    settings.BluetoothPairDeviceBehavior,
  ],

  properties: {
    /**
     * The version of this dialog to show: 'addDevice', 'pairDevice', or
     * 'connectError'. Must be set before the dialog is opened.
     */
    dialogId: String,
  },

  observers: [
    'dialogUpdated_(dialogId, pairingEvent)',
  ],

  open: function() {
    this.pinOrPass = '';
    this.getDialog_().showModal();
    this.itemWasFocused_ = false;
  },

  close: function() {
    var dialog = this.getDialog_();
    if (dialog.open)
      dialog.close();
  },

  /** @private */
  dialogUpdated_: function() {
    if (this.showEnterPincode_())
      this.$$('#pincode').focus();
    else if (this.showEnterPasskey_())
      this.$$('#passkey').focus();
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_: function() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /**
   * @param {string} dialogId
   * @return {string} The title of for that |dialogId|.
   */
  getTitle_: function(dialogId) {
    if (dialogId == 'addDevice')
      return this.i18n('bluetoothAddDevicePageTitle');
    // Use the 'pair' title for the pairing dialog and error dialog.
    return this.i18n('bluetoothPairDevicePageTitle');
  },

  /**
   * @param {string} desiredDialogType
   * @return {boolean}
   * @private
   */
  isDialogType_: function(desiredDialogType, currentDialogType) {
    return currentDialogType == desiredDialogType;
  },

  /** @private */
  onCancelTap_: function() {
    this.getDialog_().cancel();
  },

  /** @private */
  onDialogCanceled_: function() {
    if (this.dialogId == 'pairDevice')
      this.sendResponse_(chrome.bluetoothPrivate.PairingResponse.CANCEL);
  },
});

})();
