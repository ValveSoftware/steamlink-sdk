// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Login UI header bar implementation.
 */

cr.define('login', function() {
  /**
   * Creates a header bar element.
   *
   * @constructor
   * @extends {HTMLDivElement}
   */
  var HeaderBar = cr.ui.define('div');

  HeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether guest button should be shown when header bar is in normal mode.
    showGuest_: false,

    // Current UI state of the sign-in screen.
    signinUIState_: SIGNIN_UI_STATE.HIDDEN,

    // Whether to show kiosk apps menu.
    hasApps_: false,

    /** @override */
    decorate: function() {
      $('shutdown-header-bar-item').addEventListener('click',
          this.handleShutdownClick_);
      $('shutdown-button').addEventListener('click',
          this.handleShutdownClick_);
      $('add-user-button').addEventListener('click',
          this.handleAddUserClick_);
      $('cancel-add-user-button').addEventListener('click',
          this.handleCancelAddUserClick_);
      $('guest-user-header-bar-item').addEventListener('click',
          this.handleGuestClick_);
      $('guest-user-button').addEventListener('click',
          this.handleGuestClick_);
      $('sign-out-user-button').addEventListener('click',
          this.handleSignoutClick_);
      $('cancel-multiple-sign-in-button').addEventListener('click',
          this.handleCancelMultipleSignInClick_);
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.LOGIN ||
          Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE) {
        if (Oobe.getInstance().newKioskUI)
          chrome.send('initializeKioskApps');
        else
          login.AppsMenuButton.decorate($('show-apps-button'));
      }
      this.updateUI_();
    },

    /**
     * Tab index value for all button elements.
     *
     * @type {number}
     */
    set buttonsTabIndex(tabIndex) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.tabIndex = tabIndex;
      }
    },

    /**
     * Disables the header bar and all of its elements.
     *
     * @type {boolean}
     */
    set disabled(value) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i)
        if (!button.classList.contains('button-restricted'))
          button.disabled = value;
    },

    /**
     * Add user button click handler.
     *
     * @private
     */
    handleAddUserClick_: function(e) {
      Oobe.showSigninUI();
      // Prevent further propagation of click event. Otherwise, the click event
      // handler of document object will set wallpaper to user's wallpaper when
      // there is only one existing user. See http://crbug.com/166477
      e.stopPropagation();
    },

    /**
     * Cancel add user button click handler.
     *
     * @private
     */
    handleCancelAddUserClick_: function(e) {
      // Let screen handle cancel itself if that is capable of doing so.
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.cancel) {
        Oobe.getInstance().currentScreen.cancel();
        return;
      }

      $('pod-row').loadLastWallpaper();

      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
      Oobe.resetSigninUI(true);
    },

    /**
     * Guest button click handler.
     *
     * @private
     */
    handleGuestClick_: function(e) {
      Oobe.disableSigninUI();
      chrome.send('launchIncognito');
      e.stopPropagation();
    },

    /**
     * Sign out button click handler.
     *
     * @private
     */
    handleSignoutClick_: function(e) {
      this.disabled = true;
      chrome.send('signOutUser');
      e.stopPropagation();
    },

    /**
     * Shutdown button click handler.
     *
     * @private
     */
    handleShutdownClick_: function(e) {
      chrome.send('shutdownSystem');
      e.stopPropagation();
    },

    /**
     * Cancel user adding button handler.
     *
     * @private
     */
    handleCancelMultipleSignInClick_: function(e) {
      chrome.send('cancelUserAdding');
      e.stopPropagation();
    },

    /**
     * If true then "Browse as Guest" button is shown.
     *
     * @type {boolean}
     */
    set showGuestButton(value) {
      this.showGuest_ = value;
      this.updateUI_();
    },

    /**
     * Current header bar UI / sign in state.
     *
     * @type {number} state Current state of the sign-in screen (see
     *       SIGNIN_UI_STATE).
     */
    get signinUIState() {
      return this.signinUIState_;
    },
    set signinUIState(state) {
      this.signinUIState_ = state;
      this.updateUI_();
    },

    /**
     * Whether the Cancel button is enabled during Gaia sign-in.
     *
     * @type {boolean}
     */
    set allowCancel(value) {
      this.allowCancel_ = value;
      this.updateUI_();
    },

    /**
     * Update whether there are kiosk apps.
     *
     * @type {boolean}
     */
    set hasApps(value) {
      this.hasApps_ = value;
      this.updateUI_();
    },

    /**
     * Updates visibility state of action buttons.
     *
     * @private
     */
    updateUI_: function() {
      var gaiaIsActive = (this.signinUIState_ == SIGNIN_UI_STATE.GAIA_SIGNIN);
      var accountPickerIsActive =
          (this.signinUIState_ == SIGNIN_UI_STATE.ACCOUNT_PICKER);
      var managedUserCreationDialogIsActive =
          (this.signinUIState_ == SIGNIN_UI_STATE.MANAGED_USER_CREATION_FLOW);
      var wrongHWIDWarningIsActive =
          (this.signinUIState_ == SIGNIN_UI_STATE.WRONG_HWID_WARNING);
      var isSamlPasswordConfirm =
          (this.signinUIState_ == SIGNIN_UI_STATE.SAML_PASSWORD_CONFIRM);
      var isMultiProfilesUI =
          (Oobe.getInstance().displayType == DISPLAY_TYPE.USER_ADDING);
      var isLockScreen =
          (Oobe.getInstance().displayType == DISPLAY_TYPE.LOCK);

      $('add-user-button').hidden =
          !accountPickerIsActive || isMultiProfilesUI || isLockScreen;
      $('cancel-add-user-button').hidden = accountPickerIsActive ||
          !this.allowCancel_ ||
          wrongHWIDWarningIsActive ||
          isMultiProfilesUI;
      $('guest-user-header-bar-item').hidden = gaiaIsActive ||
          managedUserCreationDialogIsActive ||
          !this.showGuest_ ||
          wrongHWIDWarningIsActive ||
          isSamlPasswordConfirm ||
          isMultiProfilesUI;
      $('sign-out-user-item').hidden = !isLockScreen;

      $('add-user-header-bar-item').hidden =
          $('add-user-button').hidden && $('cancel-add-user-button').hidden;
      $('apps-header-bar-item').hidden = !this.hasApps_ ||
          (!gaiaIsActive && !accountPickerIsActive);
      $('cancel-multiple-sign-in-item').hidden = !isMultiProfilesUI;

      if (!Oobe.getInstance().newKioskUI) {
        if (!$('apps-header-bar-item').hidden)
          $('show-apps-button').didShow();
      }
    },

    /**
     * Animates Header bar to hide from the screen.
     *
     * @param {function()} callback will be called once animation is finished.
     */
    animateOut: function(callback) {
      var launcher = this;
      launcher.addEventListener(
          'webkitTransitionEnd', function f(e) {
            launcher.removeEventListener('webkitTransitionEnd', f);
            callback();
          });
      // Guard timer for 2 seconds + 200 ms + epsilon.
      ensureTransitionEndEvent(launcher, 2250);

      this.classList.remove('login-header-bar-animate-slow');
      this.classList.add('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-hidden');
    },

    /**
     * Animates Header bar to slowly appear on the screen.
     *
     * @param {function()} callback will be called once animation is finished.
     */
    animateIn: function(callback) {
      if (callback) {
        var launcher = this;
        launcher.addEventListener(
            'webkitTransitionEnd', function f(e) {
              launcher.removeEventListener('webkitTransitionEnd', f);
              callback();
            });
        // Guard timer for 2 seconds + 200 ms + epsilon.
        ensureTransitionEndEvent(launcher, 2250);
      }

      if (Oobe.getInstance().displayType == DISPLAY_TYPE.OOBE) {
        this.classList.remove('login-header-bar-animate-slow');
        this.classList.add('login-header-bar-animate-fast');
      } else {
        this.classList.remove('login-header-bar-animate-fast');
        this.classList.add('login-header-bar-animate-slow');
      }

      this.classList.remove('login-header-bar-hidden');
    },
  };

  /**
   * Convenience wrapper of animateOut.
   */
  HeaderBar.animateOut = function(callback) {
    $('login-header-bar').animateOut(callback);
  };

  /**
   * Convenience wrapper of animateIn.
   */
  HeaderBar.animateIn = function(callback) {
    $('login-header-bar').animateIn(callback);
  }

  return {
    HeaderBar: HeaderBar
  };
});
