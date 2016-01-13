// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Password changed screen implementation.
 */

login.createScreen('PasswordChangedScreen', 'password-changed', function() {
  return {
    EXTERNAL_API: [
      'show'
    ],

    /** @override */
    decorate: function() {
      $('old-password').addEventListener(
        'keydown', function(e) {
          $('password-changed').classList.remove('password-error');
          if (e.keyIdentifier == 'Enter') {
            $('password-changed').migrate();
            e.stopPropagation();
          }
        });
      $('old-password').addEventListener(
        'keyup', function(e) {
          if ($('password-changed').disabled)
            return;
          $('password-changed-ok-button').disabled = this.value.length == 0;
        });
      $('password-changed-cant-remember-link').addEventListener(
        'click', function(e) {
          if (this.classList.contains('disabled'))
            return;
          var screen = $('password-changed');
          if (screen.classList.contains('migrate')) {
            screen.classList.remove('migrate');
            screen.classList.add('resync');
            $('password-changed-proceed-button').focus();
            $('password-changed').classList.remove('password-error');
            $('old-password').value = '';
            $('password-changed-ok-button').disabled = true;
          }
        });
    },

    /**
     * Screen controls.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var backButton = this.ownerDocument.createElement('button');
      backButton.id = 'password-changed-back-button';
      backButton.textContent =
          loadTimeData.getString('passwordChangedBackButton');
      backButton.addEventListener('click', function(e) {
        var screen = $('password-changed');
        if (screen.classList.contains('migrate')) {
          screen.cancel();
        } else {
          // Resync all data UI step.
          screen.classList.remove('resync');
          screen.classList.add('migrate');
          $('old-password').focus();
        }
        e.stopPropagation();
      });
      buttons.push(backButton);

      var okButton = this.ownerDocument.createElement('button');
      okButton.id = 'password-changed-ok-button';
      okButton.textContent = loadTimeData.getString('passwordChangedsOkButton');
      okButton.addEventListener('click', function(e) {
        $('password-changed').migrate();
        e.stopPropagation();
      });
      buttons.push(okButton);

      var proceedAnywayButton = this.ownerDocument.createElement('button');
      proceedAnywayButton.id = 'password-changed-proceed-button';
      proceedAnywayButton.textContent =
          loadTimeData.getString('proceedAnywayButton');
      proceedAnywayButton.addEventListener('click', function(e) {
        var screen = $('password-changed');
        if (screen.classList.contains('resync'))
          $('password-changed').resync();
        e.stopPropagation();
      });
      buttons.push(proceedAnywayButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('old-password');
    },

    /**
     * True if the the screen is disabled (handles no user interaction).
     * @type {boolean}
     */
    disabled_: false,
    get disabled() {
      return this.disabled_;
    },
    set disabled(value) {
      this.disabled_ = value;
      var controls = this.querySelectorAll('button,input');
      for (var i = 0, control; control = controls[i]; ++i) {
        control.disabled = value;
      }
      $('login-header-bar').disabled = value;
      $('password-changed-cant-remember-link').classList[
          value ? 'add' : 'remove']('disabled');
    },

    /**
     * Cancels password migration and drops the user back to the login screen.
     */
    cancel: function() {
      this.disabled = true;
      chrome.send('cancelPasswordChangedFlow');
    },

    /**
     * Starts migration process using old password that user provided.
     */
    migrate: function() {
      if (!$('old-password').value) {
        $('old-password').focus();
        return;
      }
      this.disabled = true;
      chrome.send('migrateUserData', [$('old-password').value]);
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      $('login-header-bar').disabled = false;
    },

    /**
     * Starts migration process by removing old cryptohome and re-syncing data.
     */
    resync: function() {
      this.disabled = true;
      chrome.send('resyncUserData');
    },

    /**
     * Show password changed screen.
     * @param {boolean} showError Whether to show the incorrect password error.
     */
    show: function(showError) {
      // We'll get here after the successful online authentication.
      // It assumes session is about to start so hides login screen controls.
      Oobe.getInstance().headerHidden = false;
      var screen = $('password-changed');
      screen.classList.toggle('password-error', showError);
      screen.classList.add('migrate');
      screen.classList.remove('resync');
      $('old-password').value = '';
      $('password-changed').disabled = false;

      Oobe.showScreen({id: SCREEN_PASSWORD_CHANGED});
      $('password-changed-ok-button').disabled = true;
    }
  };
});

