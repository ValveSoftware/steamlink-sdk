// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-password-prompt-dialog' shows a dialog which asks for the user to
 * enter their password. It validates the password is correct. Once the user has
 * entered their account password, the page fires an 'authenticated' event and
 * updates the setModes binding.
 *
 * The setModes binding is a wrapper around chrome.quickUnlockPrivate.setModes
 * which has a prebound account password. The account password by itself is not
 * available for other elements to access.
 *
 * Example:
 *
 * <settings-password-prompt-dialog
 *   id="passwordPrompt"
 *   set-modes="[[setModes]]">
 * </settings-password-prompt-dialog>
 *
 * this.$.passwordPrompt.open()
 */

(function() {
'use strict';

/** @const */ var PASSWORD_ACTIVE_DURATION_MS = 10 * 60 * 1000; // Ten minutes.

Polymer({
  is: 'settings-password-prompt-dialog',

  properties: {
    /**
     * A wrapper around chrome.quickUnlockPrivate.setModes with the account
     * password already supplied. If this is null, the authentication screen
     * needs to be redisplayed. This property will be cleared after
     * |this.passwordActiveDurationMs_| milliseconds.
     */
    setModes: {
      type: Object,
      notify: true
    },

    /**
     * The actual value of the password field. This is cleared whenever the
     * authentication screen is not displayed so that the user's password is not
     * easily available to an attacker. The actual password is stored as an
     * captured closure variable inside of setModes.
     * @private
     */
    password_: {
      type: String,
      observer: 'onPasswordChanged_'
    },

    /**
     * Helper property which marks password as valid/invalid.
     * @private
     */
    passwordInvalid_: Boolean,

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overriden by tests.
     * @private
     */
    quickUnlockPrivate_: {
      type: Object,
      value: chrome.quickUnlockPrivate
    },

    /**
     * PASSWORD_ACTIVE_DURATION_MS value. May be overridden by tests.
     * @private
     */
    passwordActiveDurationMs_: {
      type: Number,
      value: PASSWORD_ACTIVE_DURATION_MS
    },
  },

  /**
   * Open up the dialog. This will wait until the dialog has loaded before
   * opening it.
   */
  open: function() {
    // Wait until the dialog is attached to the DOM before trying to open it.
    var dialog = /** @type {{isConnected: boolean}} */ (this.$.dialog);
    if (!dialog.isConnected) {
      setTimeout(this.open.bind(this));
      return;
    }

    if (this.$.dialog.open)
      return;

    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_: function() {
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * Called whenever the dialog is closed.
   * @private
   */
  onClose_: function() {
    this.password_ = '';
  },

  /** @private */
  onKeydown_: function(e) {
    if (e.key == 'Enter')
      this.submitPassword_();
  },

  /**
   * Run the account password check.
   * @private
   */
  submitPassword_: function() {
    clearTimeout(this.clearAccountPasswordTimeout_);

    // The user might have started entering a password and then deleted it all.
    // Do not submit/show an error in this case.
    if (!this.password_) {
      this.passwordInvalid_ = false;
      return;
    }

    function onPasswordChecked(valid) {
      // The password might have been cleared during the duration of the
      // getActiveModes call.
      this.passwordInvalid_ = !valid && !!this.password_;

      if (valid) {
        // Create the |this.setModes| closure and automatically clear it after
        // |this.passwordActiveDurationMs_|.
        var password = this.password_;
        this.password_ = '';

        this.setModes = function(modes, credentials, onComplete) {
          this.quickUnlockPrivate_.setModes(
              password, modes, credentials, onComplete);
        }.bind(this);

        function clearSetModes() {
          // Reset the password so that any cached references to this.setModes
          // will fail.
          password = '';
          this.setModes = null;
        }

        this.clearAccountPasswordTimeout_ = setTimeout(
          clearSetModes.bind(this), this.passwordActiveDurationMs_);

        // Clear stored password state and close the dialog.
        this.password_ = '';
        if (this.$.dialog.open)
          this.$.dialog.close();
      }
    }

    this.checkAccountPassword_(onPasswordChecked.bind(this));
  },

  /** @private */
  onPasswordChanged_: function() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  enableConfirm_: function() {
    return !!this.password_ && !this.passwordInvalid_;
  },

  /**
  * Helper method that checks if the current password is valid.
  * @param {function(boolean):void} onCheck
  */
  checkAccountPassword_: function(onCheck) {
    // We check the account password by trying to update the active set of quick
    // unlock modes without changing any credentials.
    this.quickUnlockPrivate_.getActiveModes(function(modes) {
      var credentials =
          /** @type {!Array<string>} */ (Array(modes.length).fill(''));
      this.quickUnlockPrivate_.setModes(
          this.password_, modes, credentials, onCheck);
    }.bind(this));
  }
});

})();
