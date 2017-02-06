// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * access points.
 */
(function() {

/** @enum {string} */
var ErrorType = {
  NONE: 'none',
  INCORRECT_PIN: 'incorrect-pin',
  INCORRECT_PUK: 'incorrect-puk',
  MISMATCHED_PIN: 'mismatched-pin',
  INVALID_PIN: 'invalid-pin',
  INVALID_PUK: 'invalid-puk'
};

var PIN_MIN_LENGTH = 4;
var PUK_MIN_LENGTH = 8;

Polymer({
  is: 'network-siminfo',

  properties: {
    /**
     * The network properties associated with the element.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_'
    },

    /** Set to true when a PUK is required to unlock the SIM. */
    pukRequired: {
      type: Boolean,
      value: false,
      observer: 'pukRequiredChanged_'
    },

    /**
     * Set to an ErrorType value after an incorrect PIN or PUK entry.
     * @type {ErrorType}
     */
    error: {
      type: Object,
      value: ErrorType.NONE
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
    },
  },

  sendSimLockEnabled_: false,

  /** Polymer networkProperties changed method. */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;
    var simLockStatus = this.networkProperties.Cellular.SIMLockStatus;
    this.pukRequired =
        !!simLockStatus && simLockStatus.LockType == CrOnc.LockType.PUK;
  },

  /** Polymer networkProperties changed method. */
  pukRequiredChanged_: function() {
    if (this.$.unlockPukDialog.opened) {
      if (this.pukRequired)
        this.$.unlockPuk.focus();
      else
        this.$.unlockPukDialog.close();
      return;
    }

    if (!this.pukRequired)
      return;

    // If the PUK was activated while attempting to enter or change a pin,
    // close the dialog and open the unlock PUK dialog.
    var showUnlockPuk = false;
    if (this.$.enterPinDialog.opened) {
      this.$.enterPinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.changePinDialog.opened) {
      this.$.changePinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.unlockPinDialog.opened) {
      this.$.unlockPinDialog.close();
      showUnlockPuk = true;
    }
    if (!showUnlockPuk)
      return;

    this.error = ErrorType.NONE;
    this.$.unlockPukDialog.open();
    this.$.unlockPuk.focus();
  },

  /** Polymer networkProperties changed method. */
  onSimLockEnabledChange_: function(event) {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;
    this.sendSimLockEnabled_ = event.target.checked;
    this.error = ErrorType.NONE;
    this.$.enterPinDialog.open();
  },

  /**
   * Focuses the correct element when the dialog is opened.
   * @param {Event} event
   * @private
   */
  onEnterPinDialogOpened_: function(event) {
    this.$.enterPin.value = '';
    this.$.enterPin.focus();
  },

  /**
   * Sends the PIN value from the Enter PIN dialog.
   * @param {Event} event
   * @private
   */
  sendEnterPin_: function(event) {
    var guid = this.networkProperties && this.networkProperties.GUID;
    if (!guid)
      return;

    var pin = this.$.enterPin.value;
    if (!this.validatePin_(pin))
      return;

    var simState = /** @type {!CrOnc.CellularSimState} */({
      currentPin: pin,
      requirePin: this.sendSimLockEnabled_
    });
    this.networkingPrivate.setCellularSimState(guid, simState, function() {
      if (chrome.runtime.lastError) {
        this.error = ErrorType.INCORRECT_PIN;
      } else {
        this.error = ErrorType.NONE;
        this.$.enterPinDialog.close();
      }
    }.bind(this));
  },

  /**
   * Opens the Change PIN dialog.
   * @param {Event} event
   * @private
   */
  onChangePinTap_: function(event) {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;
    this.error = ErrorType.NONE;
    this.$.changePinDialog.open();
  },

  /**
   * Focuses the correct element when the dialog is opened.
   * @param {Event} event
   * @private
   */
  onChangePinDialogOpened_: function(event) {
    this.$.changePinOld.value = '';
    this.$.changePinNew1.value = '';
    this.$.changePinNew2.value = '';
    this.$.changePinOld.focus();
  },

  /**
   * Sends the old and new PIN values from the Change PIN dialog.
   * @param {Event} event
   * @private
   */
  sendChangePin_: function(event) {
    var guid = this.networkProperties && this.networkProperties.GUID;
    if (!guid)
      return;

    var newPin = this.$.changePinNew1.value;
    if (!this.validatePin_(newPin, this.$.changePinNew2.value))
      return;

    var simState = /** @type {!CrOnc.CellularSimState} */({
      requirePin: true,
      currentPin: this.$.changePinOld.value,
      newPin: newPin
    });
    this.networkingPrivate.setCellularSimState(guid, simState, function() {
      if (chrome.runtime.lastError) {
        this.error = ErrorType.INCORRECT_PIN;
      } else {
        this.error = ErrorType.NONE;
        this.$.changePinDialog.close();
      }
    }.bind(this));
  },

  /**
   * Opens the Unlock PIN dialog.
   * @param {Event} event
   * @private
   */
  onUnlockPinTap_: function(event) {
    this.error = ErrorType.NONE;
    this.$.unlockPinDialog.open();
  },

  /**
   * Focuses the correct element when the dialog is opened.
   * @param {Event} event
   * @private
   */
  onUnlockPinDialogOpened_: function(event) {
    this.$.unlockPin.value = '';
    this.$.unlockPin.focus();
  },

  /**
   * Sends the PIN value from the Unlock PIN dialog.
   * @param {Event} event
   * @private
   */
  sendUnlockPin_: function(event) {
    var guid = this.networkProperties && this.networkProperties.GUID;
    if (!guid)
      return;
    var pin = this.$.unlockPin.value;
    if (!this.validatePin_(pin))
      return;

    this.networkingPrivate.unlockCellularSim(guid, pin, '', function() {
      if (chrome.runtime.lastError) {
        this.error = ErrorType.INCORRECT_PIN;
      } else {
        this.error = ErrorType.NONE;
        this.$.unlockPinDialog.close();
      }
    }.bind(this));
  },

  /**
   * Opens the Unlock PUK dialog.
   * @param {Event} event
   * @private
   */
  unlockPuk_: function(event) {
    this.error = ErrorType.NONE;
    this.$.unlockPukDialog.open();
  },

  /**
   * Focuses the correct element when the dialog is opened.
   * @param {Event} event
   * @private
   */
  onUnlockPukDialogOpened_: function(event) {
    this.$.unlockPuk.value = '';
    this.$.unlockPin1.value = '';
    this.$.unlockPin2.value = '';
    this.$.unlockPuk.focus();
  },

  /**
   * Sends the PUK value and new PIN value from the Unblock PUK dialog.
   * @param {Event} event
   * @private
   */
  sendUnlockPuk_: function(event) {
    var guid = this.networkProperties && this.networkProperties.GUID;
    if (!guid)
      return;

    var puk = this.$.unlockPuk.value;
    if (!this.validatePuk_(puk))
      return;
    var pin = this.$.unlockPin1.value;
    if (!this.validatePin_(pin, this.$.unlockPin2.value))
      return;

    this.networkingPrivate.unlockCellularSim(guid, pin, puk, function() {
      if (chrome.runtime.lastError) {
        this.error = ErrorType.INCORRECT_PUK;
      } else {
        this.error = ErrorType.NONE;
        this.$.unlockSimDialog.close();
      }
    }.bind(this));
  },

  /**
   * @param {!CrOnc.NetworkProperties|undefined} networkProperties
   * @return {boolean} True if the Cellular SIM is locked.
   * @private
   */
  isSimLocked_: function(networkProperties) {
    return !!networkProperties && CrOnc.isSimLocked(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties|undefined} networkProperties
   * @return {string} The message for the number of retries left.
   * @private
   */
  getRetriesLeftMsg_: function(networkProperties) {
    var retriesLeft =
        this.get('Cellular.SIMLockStatus.RetriesLeft', networkProperties) || 0;
    // TODO(stevenjb): Localize
    return 'Retries left: ' + retriesLeft.toString();
  },

  /**
   * @param {string} error
   * @return {boolean} True if an error message should be shown for |error|.
   * @private
   */
  showError_: function(error) {
    return !!error && error != ErrorType.NONE;
  },

  /**
   * @param {string} error
   * @return {string} The error message to display for |error|.
   * @private
   */
  getErrorMsg_: function(error) {
    // TODO(stevenjb_: Translate
    if (error == ErrorType.INCORRECT_PIN)
      return 'Incorrect PIN.';
    if (error == ErrorType.INCORRECT_PUK)
      return 'Incorrect PUK.';
    if (error == ErrorType.MISMATCHED_PIN)
      return 'PIN values do not match.';
    if (error == ErrorType.INVALID_PIN)
      return 'Invalid PIN.';
    if (error == ErrorType.INVALID_PUK)
      return 'Invalid PUK.';
    return '';
  },

  /**
   * Checks whether |pin1| is of the proper length and if opt_pin2 is not
   * undefined, whether pin1 and opt_pin2 match. On any failure, sets
   * |this.error| and returns false.
   * @param {string} pin1
   * @param {string=} opt_pin2
   * @return {boolean} True if the pins match and are of minimum length.
   * @private
   */
  validatePin_: function(pin1, opt_pin2) {
    if (pin1.length < PIN_MIN_LENGTH) {
      this.error = ErrorType.INVALID_PIN;
      return false;
    }
    if (opt_pin2 != undefined && pin1 != opt_pin2) {
      this.error = ErrorType.MISMATCHED_PIN;
      return false;
    }
    return true;
  },

  /**
   * Checks whether |puk| is of the proper length. If not, sets |this.error|
   * and returns false.
   * @param {string} puk
   * @return {boolean} True if the puk is of minimum length.
   * @private
   */
  validatePuk_: function(puk) {
    if (puk.length < PUK_MIN_LENGTH) {
      this.error = ErrorType.INVALID_PUK;
      return false;
    }
    return true;
  }
});
})();
