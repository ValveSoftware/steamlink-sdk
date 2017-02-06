// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-page' is the settings page for managing bluetooth
 *  properties and devices.
 *
 * Example:
 *    <core-animated-pages>
 *      <settings-bluetooth-page>
 *      </settings-bluetooth-page>
 *      ... other pages ...
 *    </core-animated-pages>
 */

var bluetoothPage = bluetoothPage || {
  /**
   * Set this to provide a fake implementation for testing.
   * @type {Bluetooth}
   */
  bluetoothApiForTest: null,

  /**
   * Set this to provide a fake implementation for testing.
   * @type {BluetoothPrivate}
   */
  bluetoothPrivateApiForTest: null,
};

Polymer({
  is: 'settings-bluetooth-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** Whether bluetooth is enabled. */
    bluetoothEnabled: {
      type: Boolean,
      value: false,
      observer: 'bluetoothEnabledChanged_',
    },

    /** Whether the device list is expanded. */
    deviceListExpanded: {
      type: Boolean,
      value: false,
    },

    /**
     * The cached bluetooth adapter state.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     */
    adapterState: Object,

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     */
    deviceList: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Set to the name of the dialog to show. This page uses a single
     * paper-dialog to host one of two dialog elements, 'addDevice' or
     * 'pairDevice'. This allows a seamless transition between adding and
     * pairing dialogs. Note: This property should be set before opening the
     * dialog, and setting the property will not itself cause the dialog to
     * open.
     */
    dialog: String,

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
     * Interface for bluetooth calls. May be overriden by tests.
     * @type {Bluetooth}
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. May be overriden by tests.
     * @type {BluetoothPrivate}
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },
  },

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)|undefined}
   * @private
   */
  bluetoothAdapterStateChangedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceAdded/Changed events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceUpdatedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceRemoved events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceRemovedListener_: undefined,

  /**
   * Listener for chrome.bluetoothPrivate.onPairing events.
   * @type {function(!chrome.bluetoothPrivate.PairingEvent)|undefined}
   * @private
   */
  bluetoothPrivateOnPairingListener_: undefined,

  /** @override */
  ready: function() {
    if (bluetoothPage.bluetoothApiForTest)
      this.bluetooth = bluetoothPage.bluetoothApiForTest;
    if (bluetoothPage.bluetoothPrivateApiForTest)
      this.bluetoothPrivate = bluetoothPage.bluetoothPrivateApiForTest;
  },

  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    this.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    this.bluetoothDeviceUpdatedListener_ =
        this.onBluetoothDeviceUpdated_.bind(this);
    this.bluetooth.onDeviceAdded.addListener(
        this.bluetoothDeviceUpdatedListener_);
    this.bluetooth.onDeviceChanged.addListener(
        this.bluetoothDeviceUpdatedListener_);

    this.bluetoothDeviceRemovedListener_ =
        this.onBluetoothDeviceRemoved_.bind(this);
    this.bluetooth.onDeviceRemoved.addListener(
        this.bluetoothDeviceRemovedListener_);

    // Request the inital adapter state.
    this.bluetooth.getAdapterState(
        this.bluetoothAdapterStateChangedListener_);
  },

  /** @override */
  detached: function() {
    if (this.bluetoothAdapterStateChangedListener_) {
      this.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
    if (this.bluetoothDeviceUpdatedListener_) {
      this.bluetooth.onDeviceAdded.removeListener(
          this.bluetoothDeviceUpdatedListener_);
      this.bluetooth.onDeviceChanged.removeListener(
          this.bluetoothDeviceUpdatedListener_);
    }
    if (this.bluetoothDeviceRemovedListener_) {
      this.bluetooth.onDeviceRemoved.removeListener(
          this.bluetoothDeviceRemovedListener_);
    }
  },

  bluetoothEnabledChanged_: function() {
    // When bluetooth is enabled, auto-expand the device list.
    if (this.bluetoothEnabled)
      this.deviceListExpanded = true;
  },

  /**
   * @param {boolean} bluetoothEnabled
   * @param {boolean} deviceListExpanded
   * @return {boolean} Whether the <iron-collapse> can be shown.
   * @private
   */
  canShowDeviceList_: function(bluetoothEnabled, deviceListExpanded) {
    return bluetoothEnabled && deviceListExpanded;
  },

  /**
   * If bluetooth is enabled, request the complete list of devices and update
   * |deviceList|.
   * @private
   */
  updateDeviceList_: function() {
    if (!this.bluetoothEnabled) {
      this.deviceList = [];
      return;
    }
    this.bluetooth.getDevices(function(devices) {
      this.deviceList = devices;
    }.bind(this));
  },

  /**
   * Event called when a user action changes the bluetoothEnabled state.
   * @private
   */
  onBluetoothEnabledChange_: function() {
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothEnabled}, function() {
          if (chrome.runtime.lastError) {
            console.error(
                'Error enabling bluetooth: ' +
                chrome.runtime.lastError.message);
          }
        });
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.adapterState = state;
    this.bluetoothEnabled = state.powered;
    this.updateDeviceList_();
  },

  /**
   * Process bluetooth.onDeviceAdded and onDeviceChanged events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceUpdated_: function(device) {
    var address = device.address;
    if (this.dialog && this.pairingDevice &&
        this.pairingDevice.address == address) {
      this.pairingDevice = device;
    }
    var index = this.getDeviceIndex_(address);
    if (index >= 0) {
      // Use splice to update the item in order to update the dom-repeat lists.
      // See https://github.com/Polymer/polymer/issues/3254.
      this.splice('deviceList', index, 1, device);
      return;
    }
    this.push('deviceList', device);
  },

  /**
   * Process bluetooth.onDeviceRemoved events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceRemoved_: function(device) {
    var address = device.address;
    var index = this.getDeviceIndex_(address);
    if (index < 0)
      return;
    this.splice('deviceList', index, 1);
  },

  /** @private */
  startDiscovery_: function() {
    if (!this.adapterState || this.adapterState.discovering)
      return;

    if (!this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivateOnPairingListener_ =
          this.onBluetoothPrivateOnPairing_.bind(this);
      this.bluetoothPrivate.onPairing.addListener(
          this.bluetoothPrivateOnPairingListener_);
    }

    this.bluetooth.startDiscovery(function() {
      if (chrome.runtime.lastError) {
        if (chrome.runtime.lastError.message == 'Failed to stop discovery') {
          // May happen if also started elsewhere; ignore.
          return;
        }
        console.error('startDsicovery Error: ' +
                      chrome.runtime.lastError.message);
      }
    });
  },

  /** @private */
  stopDiscovery_: function() {
    if (!this.get('adapterState.discovering'))
      return;

    if (this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivate.onPairing.removeListener(
          this.bluetoothPrivateOnPairingListener_);
      this.bluetoothPrivateOnPairingListener_ = undefined;
    }

    this.bluetooth.stopDiscovery(function() {
      if (chrome.runtime.lastError) {
        console.error('Error stopping bluetooth discovery: ' +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * Process bluetoothPrivate.onPairing events.
   * @param {!chrome.bluetoothPrivate.PairingEvent} e
   * @private
   */
  onBluetoothPrivateOnPairing_: function(e) {
    if (!this.dialog || !this.pairingDevice ||
        e.device.address != this.pairingDevice.address) {
      return;
    }
    if (e.pairing == chrome.bluetoothPrivate.PairingEventType.KEYS_ENTERED &&
        e.passkey === undefined && this.pairingEvent) {
      // 'keysEntered' event might not include the updated passkey so preserve
      // the current one.
      e.passkey = this.pairingEvent.passkey;
    }
    this.pairingEvent = e;
  },

  /** @private */
  onAddDeviceTap_: function() { this.openDialog_('addDevice'); },

  /**
   * @param {!{detail: {action: string, device: !chrome.bluetooth.Device}}} e
   * @private
   */
  onDeviceEvent_: function(e) {
    var action = e.detail.action;
    var device = e.detail.device;
    if (action == 'connect')
      this.connectDevice_(device);
    else if (action == 'disconnect')
      this.disconnectDevice_(device);
    else if (action == 'remove')
      this.forgetDevice_(device);
    else
      console.error('Unexected action: ' + action);
  },

  /**
   * Handle a response sent from the pairing dialog and pass it to the
   * bluetoothPrivate API.
   * @param {Event} e
   * @private
   */
  onResponse_: function(e) {
    var options =
        /** @type {!chrome.bluetoothPrivate.SetPairingResponseOptions} */ (
            e.detail);
    this.bluetoothPrivate.setPairingResponse(options, function() {
      if (chrome.runtime.lastError) {
        // TODO(stevenjb): Show error.
        console.error(
            'Error setting pairing response: ' + options.device.name +
            ': Response: ' + options.response + ': Error: ' +
            chrome.runtime.lastError.message);
      }
      this.closeDialog_();
    }.bind(this));
  },

  /**
   * @param {string} address
   * @return {number} The index of the device associated with |address| or -1.
   * @private
   */
  getDeviceIndex_: function(address) {
    var len = this.deviceList.length;
    for (var i = 0; i < len; ++i) {
      if (this.deviceList[i].address == address)
        return i;
    }
    return -1;
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
  deviceIsPairedOrConnecting_: function(device) {
    return !!device.paired || !!device.connecting;
  },

  /**
   * @param {Object} deviceListChanges Changes to the deviceList Array.
   * @return {boolean} True if deviceList contains any paired devices.
   * @private
   */
  haveDevices_: function(deviceListChanges) {
    return this.deviceList.findIndex(function(d) { return !!d.paired; }) != -1;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  connectDevice_: function(device) {
    // If the device is not paired, show the pairing dialog.
    if (!device.paired) {
      // Set the pairing device and clear any pairing event.
      this.pairingDevice = device;
      this.pairingEvent = null;

      this.openDialog_('pairDevice');
    }

    this.bluetoothPrivate.connect(device.address, function(result) {
      if (chrome.runtime.lastError) {
        console.error(
            'Error connecting: ' + device.address +
            chrome.runtime.lastError.message);
        // TODO(stevenjb): Show error message insead.
        this.closeDialog_();
      }
    }.bind(this));
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  disconnectDevice_: function(device) {
    this.bluetoothPrivate.disconnectAll(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error disconnecting: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  forgetDevice_: function(device) {
    this.bluetoothPrivate.forgetDevice(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error forgetting: ' + device.name + ': ' +
                chrome.runtime.lastError.message);
      }
      this.updateDeviceList_();
    }.bind(this));
  },

  /**
   * @param {string} dialog
   * @param {string} dialogToShow The name of the dialog.
   * @return {boolean}
   * @private
   */
  dialogIsVisible_(dialog, dialogToShow) {
    return dialogToShow == dialog;
  },

  /**
   * @param {string} dialogId
   * @private
   */
  openDialog_: function(dialogId) {
    if (this.dialog) {
      // Dialog already opened, just update the contents.
      this.dialog = dialogId;
      return;
    }
    this.dialog = dialogId;
    // Call flush so that the dialog gets sized correctly before it is opened.
    Polymer.dom.flush();
    var dialog = this.$$('#deviceDialog');
    dialog.open();
    dialog.focus();
  },

  /** @private */
  closeDialog_: function() {
    if (!this.dialog)
      return;
    var dialog = this.$$('#deviceDialog');
    dialog.close();
    this.dialog = '';
    this.pairingDevice = null;
    this.pairingEvent = null;
  },

  /** @private */
  onCloseDialog_: function(event) { this.closeDialog_(); },

  /** @private */
  onDialogOpened_: function() { this.startDiscovery_(); },

  /** @private */
  onDialogClosed_: function() { this.stopDiscovery_(); },
});
