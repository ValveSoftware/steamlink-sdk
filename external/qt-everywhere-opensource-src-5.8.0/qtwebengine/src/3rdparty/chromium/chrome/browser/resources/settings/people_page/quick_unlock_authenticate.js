// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-quick-unlock-authenticate' shows a password input prompt to the
 * user. It validates the password is correct. Once the user has entered their
 * account password, the page navigates to the quick unlock setup methods page.
 *
 * This element provides a wrapper around chrome.quickUnlockPrivate.setModes
 * which has a prebound account password (the |set-modes| property). The account
 * password by itself is not available for other elements to access.
 *
 * Example:
 *
 * <settings-quick-unlock-authenticate
 *   set-modes="[[setModes]]"
 *   current-route="{{currentRoute}}"
 *   profile-name="[[profileName_]]">
 * </settings-quick-unlock-authenticate>
 */

(function() {
'use strict';

/** @const */ var PASSWORD_ACTIVE_DURATION_MS = 10 * 60 * 1000; // Ten minutes.
/** @const */ var AUTOSUBMIT_DELAY_MS = 500; // .5 seconds

/**
 * Helper method that checks if |password| is valid.
 * @param {string} password
 * @param {function(boolean):void} onCheck
 */
function checkAccountPassword_(password, onCheck) {
  // We check the account password by trying to update the active set of quick
  // unlock modes without changing any credentials.
  chrome.quickUnlockPrivate.getActiveModes(function(modes) {
    var credentials =
        /** @type {!Array<string>} */ (Array(modes.length).fill(''));
    chrome.quickUnlockPrivate.setModes(password, modes, credentials, onCheck);
  });
}

Polymer({
  is: 'settings-quick-unlock-authenticate',

  behaviors: [
    QuickUnlockRoutingBehavior,
  ],

  properties: {
    /**
     * A wrapper around chrome.quickUnlockPrivate.setModes with the account
     * password already supplied. If this is null, the authentication screen
     * needs to be redisplayed. This property will be cleared after
     * PASSWORD_ACTIVE_DURATION_MS milliseconds.
     */
    setModes: {
      type: Object,
      notify: true
    },

    /**
     * Name of the profile.
     */
    profileName: String,

    /**
     * The actual value of the password field. This is cleared whenever the
     * authentication screen is not displayed so that the user's password is not
     * easily available to an attacker. The actual password is stored as an
     * captured closure variable inside of setModes.
     * @private
     */
    password_: String,

    /**
     * Helper property which marks password as valid/invalid.
     * @private
     */
    passwordInvalid_: Boolean
  },

  observers: [
    'onRouteChanged_(currentRoute)'
  ],

  /** @private */
  onRouteChanged_: function(currentRoute) {
    // Clear local state if this screen is not active so if this screen shows
    // up again the user will get a fresh UI.
    if (!this.isScreenActive(QuickUnlockScreen.AUTHENTICATE)) {
      this.password_ = '';
      this.passwordInvalid_ = false;
    }
  },

  /**
   * Start or restart a timer to check the account password and move past the
   * authentication screen.
   * @private
   */
  startDelayedPasswordCheck_: function() {
    clearTimeout(this.delayedPasswordCheckTimeout_);
    this.delayedPasswordCheckTimeout_ =
        setTimeout(this.checkPasswordNow_.bind(this), AUTOSUBMIT_DELAY_MS);
  },

  /**
   * Run the account password check right now. This will cancel any delayed
   * check.
   * @private
   */
  checkPasswordNow_: function() {
    clearTimeout(this.delayedPasswordCheckTimeout_);
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
        // |PASSWORD_ACTIVE_DURATION_MS|.
        var password = this.password_;
        this.password_ = '';

        this.setModes = function(modes, credentials, onComplete) {
          chrome.quickUnlockPrivate.setModes(
              password, modes, credentials, onComplete);
        };

        function clearSetModes() {
          // Reset the password so that any cached references to this.setModes
          // will fail.
          password = '';
          this.setModes = null;
        }

        this.clearAccountPasswordTimeout_ = setTimeout(
            clearSetModes.bind(this), PASSWORD_ACTIVE_DURATION_MS);

        this.currentRoute = {
          page: 'basic',
          section: 'people',
          subpage: [QuickUnlockScreen.CHOOSE_METHOD]
        };
      }
    }

    checkAccountPassword_(this.password_, onPasswordChecked.bind(this));
  }
});

})();
