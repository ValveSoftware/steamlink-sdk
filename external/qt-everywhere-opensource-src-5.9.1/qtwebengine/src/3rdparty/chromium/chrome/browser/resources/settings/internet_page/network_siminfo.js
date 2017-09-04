// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying cellular sim info.
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
      observer: 'networkPropertiesChanged_',
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * Set to true when a PUK is required to unlock the SIM.
     * @private
     */
    pukRequired_: {
      type: Boolean,
      value: false,
      observer: 'pukRequiredChanged_',
    },

    /**
     * Set to an ErrorType value after an incorrect PIN or PUK entry.
     * @private {ErrorType}
     */
    error_: {
      type: Object,
      value: ErrorType.NONE,
    },
  },

  sendSimLockEnabled_: false,

  networkPropertiesChanged_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;
    var simLockStatus = this.networkProperties.Cellular.SIMLockStatus;
    this.pukRequired_ =
        !!simLockStatus && simLockStatus.LockType == CrOnc.LockType.PUK;
  },

  pukRequiredChanged_: function() {
    if (this.$.unlockPukDialog.open) {
      if (this.pukRequired_)
        this.$.unlockPuk.focus();
      else
        this.$.unlockPukDialog.close();
      return;
    }

    if (!this.pukRequired_)
      return;

    // If the PUK was activated while attempting to enter or change a pin,
    // close the dialog and open the unlock PUK dialog.
    var showUnlockPuk = false;
    if (this.$.enterPinDialog.open) {
      this.$.enterPinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.changePinDialog.open) {
      this.$.changePinDialog.close();
      showUnlockPuk = true;
    }
    if (this.$.unlockPinDialog.open) {
      this.$.unlockPinDialog.close();
      showUnlockPuk = true;
    }
    if (!showUnlockPuk)
      return;

    this.error_ = ErrorType.NONE;
    this.$.unlockPukDialog.showModal();
  },

  /**
   * Opens the pin dialog when the sim lock enabled state changes.
   * @param {Event} event
   * @private
   */
  onSimLockEnabledChange_: function(event) {
    if (!this.networkProperties || !this.networkProperties.Cellular)
      return;
    this.sendSimLockEnabled_ = event.target.checked;
    this.error_ = ErrorType.NONE;
    this.$.enterPinDialog.showModal();
    this.$.enterPin.value = '';
  },

  /**
   * Sends the PIN value from the Enter PIN dialog.
   * @param {Event} event
   * @private
   */
  sendEnterPin_: function(event) {
    var guid = (this.networkProperties && this.networkProperties.GUID) || '';
    var pin = this.$.enterPin.value;
    if (!this.validatePin_(pin))
      return;

    var simState = /** @type {!CrOnc.CellularSimState} */ ({
      currentPin: pin,
      requirePin: this.sendSimLockEnabled_,
    });
    this.networkingPrivate.setCellularSimState(guid, simState, function() {
      if (chrome.runtime.lastError) {
        this.error_ = ErrorType.INCORRECT_PIN;
        this.$.enterPin.inputElement.select();
      } else {
        this.error_ = ErrorType.NONE;
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
    this.error_ = ErrorType.NONE;
    this.$.changePinDialog.showModal();
    this.$.changePinOld.value = '';
    this.$.changePinNew1.value = '';
    this.$.changePinNew2.value = '';
  },

  /**
   * Sends the old and new PIN values from the Change PIN dialog.
   * @param {Event} event
   * @private
   */
  sendChangePin_: function(event) {
    var guid = (this.networkProperties && this.networkProperties.GUID) || '';
    var newPin = this.$.changePinNew1.value;
    if (!this.validatePin_(newPin, this.$.changePinNew2.value))
      return;

    var simState = /** @type {!CrOnc.CellularSimState} */ ({
      requirePin: true,
      currentPin: this.$.changePinOld.value,
      newPin: newPin
    });
    this.networkingPrivate.setCellularSimState(guid, simState, function() {
      if (chrome.runtime.lastError) {
        this.error_ = ErrorType.INCORRECT_PIN;
        this.$.changePinOld.inputElement.select();
      } else {
        this.error_ = ErrorType.NONE;
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
    this.error_ = ErrorType.NONE;
    this.$.unlockPinDialog.showModal();
    this.$.unlockPin.value = '';
  },

  /**
   * Sends the PIN value from the Unlock PIN dialog.
   * @param {Event} event
   * @private
   */
  sendUnlockPin_: function(event) {
    var guid = (this.networkProperties && this.networkProperties.GUID) || '';
    var pin = this.$.unlockPin.value;
    if (!this.validatePin_(pin))
      return;

    this.networkingPrivate.unlockCellularSim(guid, pin, '', function() {
      if (chrome.runtime.lastError) {
        this.error_ = ErrorType.INCORRECT_PIN;
        this.$.unlockPin.inputElement.select();
      } else {
        this.error_ = ErrorType.NONE;
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
    this.error_ = ErrorType.NONE;
    this.$.unlockPukDialog.showModal();
    this.$.unlockPuk.value = '';
    this.$.unlockPin1.value = '';
    this.$.unlockPin2.value = '';
  },

  /**
   * Sends the PUK value and new PIN value from the Unblock PUK dialog.
   * @param {Event} event
   * @private
   */
  sendUnlockPuk_: function(event) {
    var guid = (this.networkProperties && this.networkProperties.GUID) || '';
    var puk = this.$.unlockPuk.value;
    if (!this.validatePuk_(puk))
      return;
    var pin = this.$.unlockPin1.value;
    if (!this.validatePin_(pin, this.$.unlockPin2.value))
      return;

    this.networkingPrivate.unlockCellularSim(guid, pin, puk, function() {
      if (chrome.runtime.lastError) {
        this.error_ = ErrorType.INCORRECT_PUK;
        this.$.unlockPuk.inputElement.select();
      } else {
        this.error_ = ErrorType.NONE;
        this.$.unlockPukDialog.close();
      }
    }.bind(this));
  },

  /**
   * @return {boolean}
   * @private
   */
  showSimLocked_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular ||
        !this.networkProperties.Cellular.SIMPresent) {
      return false;
    }
    return CrOnc.isSimLocked(this.networkProperties);
  },

  /**
   * @return {boolean}
   * @private
   */
  showSimUnlocked_: function() {
    if (!this.networkProperties || !this.networkProperties.Cellular ||
        !this.networkProperties.Cellular.SIMPresent) {
      return false;
    }
    return !CrOnc.isSimLocked(this.networkProperties);
  },

  /** @private */
  getErrorMsg_: function() {
    if (this.error_ == ErrorType.NONE)
      return '';
    // TODO(stevenjb): Translate
    var msg;
    if (this.error_ == ErrorType.INCORRECT_PIN)
      msg = 'Incorrect PIN.';
    else if (this.error_ == ErrorType.INCORRECT_PUK)
      msg = 'Incorrect PUK.';
    else if (this.error_ == ErrorType.MISMATCHED_PIN)
      msg = 'PIN values do not match.';
    else if (this.error_ == ErrorType.INVALID_PIN)
      msg = 'Invalid PIN.';
    else if (this.error_ == ErrorType.INVALID_PUK)
      msg = 'Invalid PUK.';
    else
      return 'UNKNOWN ERROR';
    var retriesLeft =
        this.get(
            'Cellular.SIMLockStatus.RetriesLeft', this.networkProperties) ||
        0;
    msg += ' Retries left: ' + retriesLeft.toString();
    return msg;
  },

  /**
   * Checks whether |pin1| is of the proper length and if opt_pin2 is not
   * undefined, whether pin1 and opt_pin2 match. On any failure, sets
   * |this.error_| and returns false.
   * @param {string} pin1
   * @param {string=} opt_pin2
   * @return {boolean} True if the pins match and are of minimum length.
   * @private
   */
  validatePin_: function(pin1, opt_pin2) {
    if (pin1.length < PIN_MIN_LENGTH) {
      this.error_ = ErrorType.INVALID_PIN;
      return false;
    }
    if (opt_pin2 != undefined && pin1 != opt_pin2) {
      this.error_ = ErrorType.MISMATCHED_PIN;
      return false;
    }
    return true;
  },

  /**
   * Checks whether |puk| is of the proper length. If not, sets |this.error_|
   * and returns false.
   * @param {string} puk
   * @return {boolean} True if the puk is of minimum length.
   * @private
   */
  validatePuk_: function(puk) {
    if (puk.length < PUK_MIN_LENGTH) {
      this.error_ = ErrorType.INVALID_PUK;
      return false;
    }
    return true;
  }
});
})();
