// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Password confirmation screen implementation.
 */

login.createScreen('ConfirmPasswordScreen', 'confirm-password', function() {
  return {
    EXTERNAL_API: [
      'show'
    ],

    /**
     * Callback to run when the screen is dismissed.
     * @type {function(string)}
     */
    callback_: null,

    /** @override */
    decorate: function() {
      $('confirm-password-input').addEventListener(
          'keydown', this.onPasswordFieldKeyDown_.bind(this));
      $('confirm-password-confirm-button').addEventListener(
          'click', this.onConfirmPassword_.bind(this));
    },

    get defaultControl() {
      return $('confirm-password-input');
    },

    /** @override */
    onBeforeShow: function(data) {
      $('login-header-bar').signinUIState =
          SIGNIN_UI_STATE.SAML_PASSWORD_CONFIRM;
    },

    /**
     * Handle 'keydown' event on password input field.
     */
    onPasswordFieldKeyDown_: function(e) {
      if (e.keyIdentifier == 'Enter')
        this.onConfirmPassword_();
    },

    /**
     * Invoked when user clicks on the 'confirm' button.
     */
    onConfirmPassword_: function() {
      this.callback_($('confirm-password-input').value);
    },

    /**
     * Shows the confirm password screen.
     * @param {number} attemptCount Number of attempts tried, starting at 0.
     * @param {function(string)} callback The callback to be invoked when the
     *     screen is dismissed.
     */
    show: function(attemptCount, callback) {
      this.callback_ = callback;
      this.classList.toggle('error', attemptCount > 0);

      $('confirm-password-input').value = '';
      Oobe.showScreen({id: SCREEN_CONFIRM_PASSWORD});
      $('progress-dots').hidden = true;
    }
  };
});
