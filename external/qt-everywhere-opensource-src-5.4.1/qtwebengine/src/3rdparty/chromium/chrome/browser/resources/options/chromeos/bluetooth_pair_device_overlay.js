// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Enumeration of possible states during pairing.  The value associated with
   * each state maps to a localized string in the global variable
   * |loadTimeData|.
   * @enum {string}
   */
  var PAIRING = {
    STARTUP: 'bluetoothStartConnecting',
    ENTER_PIN_CODE: 'bluetoothEnterPinCode',
    ENTER_PASSKEY: 'bluetoothEnterPasskey',
    REMOTE_PIN_CODE: 'bluetoothRemotePinCode',
    REMOTE_PASSKEY: 'bluetoothRemotePasskey',
    CONFIRM_PASSKEY: 'bluetoothConfirmPasskey',
    CONNECT_FAILED: 'bluetoothConnectFailed',
    CANCELED: 'bluetoothPairingCanceled',
    DISMISSED: 'bluetoothPairingDismissed', // pairing dismissed(succeeded or
                                            // canceled).
  };

  /**
   * List of IDs for conditionally visible elements in the dialog.
   * @type {Array.<string>}
   * @const
   */
  var ELEMENTS = ['bluetooth-pairing-passkey-display',
                  'bluetooth-pairing-passkey-entry',
                  'bluetooth-pairing-pincode-entry',
                  'bluetooth-pair-device-connect-button',
                  'bluetooth-pair-device-cancel-button',
                  'bluetooth-pair-device-accept-button',
                  'bluetooth-pair-device-reject-button',
                  'bluetooth-pair-device-dismiss-button'];

  /**
   * Encapsulated handling of the Bluetooth device pairing page.
   * @constructor
   */
  function BluetoothPairing() {
    OptionsPage.call(this,
                     'bluetoothPairing',
                     loadTimeData.getString('bluetoothOptionsPageTabTitle'),
                     'bluetooth-pairing');
  }

  cr.addSingletonGetter(BluetoothPairing);

  BluetoothPairing.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Description of the bluetooth device.
     * @type {{name: string,
     *         address: string,
     *         paired: boolean,
     *         connected: boolean,
     *         connecting: boolean,
     *         connectable: boolean,
     *         pairing: string|undefined,
     *         passkey: number|undefined,
     *         pincode: string|undefined,
     *         entered: number|undefined}}
     * @private.
     */
    device_: null,

    /**
     * Can the dialog be programmatically dismissed.
     * @type {boolean}
     */
    dismissible_: true,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;
      $('bluetooth-pair-device-cancel-button').onclick = function() {
        OptionsPage.closeOverlay();
      };
      $('bluetooth-pair-device-reject-button').onclick = function() {
        chrome.send('updateBluetoothDevice',
                    [self.device_.address, 'reject']);
        self.device_.pairing = PAIRING.DISMISSED;
        OptionsPage.closeOverlay();
      };
      $('bluetooth-pair-device-connect-button').onclick = function() {
        var args = [self.device_.address, 'connect'];
        var passkey = self.device_.passkey;
        if (passkey)
          args.push(String(passkey));
        else if (!$('bluetooth-pairing-passkey-entry').hidden)
          args.push($('bluetooth-passkey').value);
        else if (!$('bluetooth-pairing-pincode-entry').hidden)
          args.push($('bluetooth-pincode').value);
        chrome.send('updateBluetoothDevice', args);
        // Prevent sending a 'connect' command twice.
        $('bluetooth-pair-device-connect-button').disabled = true;
      };
      $('bluetooth-pair-device-accept-button').onclick = function() {
        chrome.send('updateBluetoothDevice',
                    [self.device_.address, 'accept']);
        // Prevent sending a 'accept' command twice.
        $('bluetooth-pair-device-accept-button').disabled = true;
      };
      $('bluetooth-pair-device-dismiss-button').onclick = function() {
        OptionsPage.closeOverlay();
      };
      $('bluetooth-passkey').oninput = function() {
        var inputField = $('bluetooth-passkey');
        var value = inputField.value;
        // Note that using <input type="number"> is insufficient to restrict
        // the input as it allows negative numbers and does not limit the
        // number of charactes typed even if a range is set.  Furthermore,
        // it sometimes produces strange repaint artifacts.
        var filtered = value.replace(/[^0-9]/g, '');
        if (filtered != value)
          inputField.value = filtered;
        $('bluetooth-pair-device-connect-button').disabled =
            inputField.value.length == 0;
      }
      $('bluetooth-pincode').oninput = function() {
        $('bluetooth-pair-device-connect-button').disabled =
            $('bluetooth-pincode').value.length == 0;
      }
      $('bluetooth-passkey').addEventListener('keydown',
          this.keyDownEventHandler_.bind(this));
      $('bluetooth-pincode').addEventListener('keydown',
          this.keyDownEventHandler_.bind(this));
    },

    /** @override */
    didClosePage: function() {
      if (this.device_.pairing != PAIRING.DISMISSED &&
          this.device_.pairing != PAIRING.CONNECT_FAILED) {
        this.device_.pairing = PAIRING.CANCELED;
        chrome.send('updateBluetoothDevice',
                    [this.device_.address, 'cancel']);
      }
    },

    /**
     * Override to prevent showing the overlay if the Bluetooth device details
     * have not been specified.  Prevents showing an empty dialog if the user
     * quits and restarts Chrome while in the process of pairing with a device.
     * @return {boolean} True if the overlay can be displayed.
     */
    canShowPage: function() {
      return this.device_ && this.device_.address && this.device_.pairing;
    },

    /**
     * Sets input focus on the passkey or pincode field if appropriate.
     */
    didShowPage: function() {
      if (!$('bluetooth-pincode').hidden)
        $('bluetooth-pincode').focus();
      else if (!$('bluetooth-passkey').hidden)
        $('bluetooth-passkey').focus();
    },

    /**
     * Configures the overlay for pairing a device.
     * @param {Object} device Description of the bluetooth device.
     */
    update: function(device) {
      this.device_ = {};
      for (key in device)
        this.device_[key] = device[key];
      // Update the pairing instructions.
      var instructionsEl = $('bluetooth-pairing-instructions');
      this.clearElement_(instructionsEl);
      this.dismissible_ = ('dismissible' in device) ?
        device.dismissible : true;

      var message = loadTimeData.getString(device.pairing);
      message = message.replace('%1', this.device_.name);
      instructionsEl.textContent = message;

      // Update visibility of dialog elements.
      if (this.device_.passkey) {
        this.updatePasskey_(String(this.device_.passkey));
        if (this.device_.pairing == PAIRING.CONFIRM_PASSKEY) {
          // Confirming a match between displayed passkeys.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-accept-button',
                                 'bluetooth-pair-device-reject-button']);
          $('bluetooth-pair-device-accept-button').disabled = false;
        } else {
          // Remote entering a passkey.
          this.displayElements_(['bluetooth-pairing-passkey-display',
                                 'bluetooth-pair-device-cancel-button']);
        }
      } else if (this.device_.pincode) {
        this.updatePasskey_(String(this.device_.pincode));
        this.displayElements_(['bluetooth-pairing-passkey-display',
                               'bluetooth-pair-device-cancel-button']);
      } else if (this.device_.pairing == PAIRING.ENTER_PIN_CODE) {
        // Prompting the user to enter a PIN code.
        this.displayElements_(['bluetooth-pairing-pincode-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
        $('bluetooth-pincode').value = '';
      } else if (this.device_.pairing == PAIRING.ENTER_PASSKEY) {
        // Prompting the user to enter a passkey.
        this.displayElements_(['bluetooth-pairing-passkey-entry',
                               'bluetooth-pair-device-connect-button',
                               'bluetooth-pair-device-cancel-button']);
        $('bluetooth-passkey').value = '';
      } else if (this.device_.pairing == PAIRING.STARTUP) {
        // Starting the pairing process.
        this.displayElements_(['bluetooth-pair-device-cancel-button']);
      } else {
        // Displaying an error message.
        this.displayElements_(['bluetooth-pair-device-dismiss-button']);
      }
      // User is required to enter a passkey or pincode before the connect
      // button can be enabled.  The 'oninput' methods for the input fields
      // determine when the connect button becomes active.
      $('bluetooth-pair-device-connect-button').disabled = true;
    },

    /**
     * Handles the ENTER key for the passkey or pincode entry field.
     * @return {Event} a keydown event.
     * @private
     */
    keyDownEventHandler_: function(event) {
      /** @const */ var ENTER_KEY_CODE = 13;
      if (event.keyCode == ENTER_KEY_CODE) {
        var button = $('bluetooth-pair-device-connect-button');
        if (!button.hidden)
          button.click();
      }
    },

    /**
     * Updates the visibility of elements in the dialog.
     * @param {Array.<string>} list List of conditionally visible elements that
     *     are to be made visible.
     * @private
     */
    displayElements_: function(list) {
      var enabled = {};
      for (var i = 0; i < list.length; i++) {
        var key = list[i];
        enabled[key] = true;
      }
      for (var i = 0; i < ELEMENTS.length; i++) {
        var key = ELEMENTS[i];
        $(key).hidden = !enabled[key];
      }
    },

    /**
     * Removes all children from an element.
     * @param {!Element} element Target element to clear.
     */
    clearElement_: function(element) {
      var child = element.firstChild;
      while (child) {
        element.removeChild(child);
        child = element.firstChild;
      }
    },

    /**
     * Formats an element for displaying the passkey or PIN code.
     * @param {string} key Passkey or PIN to display.
     */
    updatePasskey_: function(key) {
      var passkeyEl = $('bluetooth-pairing-passkey-display');
      var keyClass = (this.device_.pairing == PAIRING.REMOTE_PASSKEY ||
                      this.device_.pairing == PAIRING.REMOTE_PIN_CODE) ?
          'bluetooth-keyboard-button' : 'bluetooth-passkey-char';
      this.clearElement_(passkeyEl);
      // Passkey should always have 6 digits.
      key = '000000'.substring(0, 6 - key.length) + key;
      var progress = this.device_.entered;
      for (var i = 0; i < key.length; i++) {
        var keyEl = document.createElement('span');
        keyEl.textContent = key.charAt(i);
        keyEl.className = keyClass;
        if (progress != undefined) {
          if (i < progress)
            keyEl.classList.add('key-typed');
          else if (i == progress)
            keyEl.classList.add('key-next');
          else
            keyEl.classList.add('key-untyped');
        }
        passkeyEl.appendChild(keyEl);
      }
      if (this.device_.pairing == PAIRING.REMOTE_PASSKEY ||
          this.device_.pairing == PAIRING.REMOTE_PIN_CODE) {
        // Add enter key.
        var label = loadTimeData.getString('bluetoothEnterKey');
        var keyEl = document.createElement('span');
        keyEl.textContent = label;
        keyEl.className = keyClass;
        keyEl.id = 'bluetooth-enter-key';
        if (progress != undefined) {
          if (progress > key.length)
            keyEl.classList.add('key-typed');
          else if (progress == key.length)
            keyEl.classList.add('key-next');
          else
            keyEl.classList.add('key-untyped');
        }
        passkeyEl.appendChild(keyEl);
      }
      passkeyEl.hidden = false;
    },
  };

  /**
   * Configures the device pairing instructions and displays the pairing
   * overlay.
   * @param {Object} device Description of the Bluetooth device.
   */
  BluetoothPairing.showDialog = function(device) {
    BluetoothPairing.getInstance().update(device);
    OptionsPage.showPageByName('bluetoothPairing', false);
  };

  /**
   * Displays a message from the Bluetooth adapter.
   * @param {{string: label,
   *          string: address} data  Data for constructing the message.
   */
  BluetoothPairing.showMessage = function(data) {
    var name = data.address;
    if (name.length == 0)
      return;
    var dialog = BluetoothPairing.getInstance();
    if (dialog.device_ && name == dialog.device_.address &&
        dialog.device_.pairing == PAIRING.CANCELED) {
      // Do not show any error message after cancelation of the pairing.
      return;
    }

    var list = $('bluetooth-paired-devices-list');
    if (list) {
      var index = list.find(name);
      if (index == undefined) {
        list = $('bluetooth-unpaired-devices-list');
        index = list.find(name);
      }
      if (index != undefined) {
        var entry = list.dataModel.item(index);
        if (entry && entry.name)
          name = entry.name;
      }
    }
    BluetoothPairing.showDialog({name: name,
                                 address: data.address,
                                 pairing: data.label,
                                 dismissible: false});
  };

  /**
   * Closes the Bluetooth pairing dialog.
   */
  BluetoothPairing.dismissDialog = function() {
    var overlay = OptionsPage.getTopmostVisiblePage();
    var dialog = BluetoothPairing.getInstance();
    if (overlay == dialog && dialog.dismissible_) {
      dialog.device_.pairing = PAIRING.DISMISSED;
      OptionsPage.closeOverlay();
    }
  };

  // Export
  return {
    BluetoothPairing: BluetoothPairing
  };
});
