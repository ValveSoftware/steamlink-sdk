// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Desktop User Chooser UI control bar implementation.
 */

cr.define('login', function() {
  /**
   * Creates a header bar element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var HeaderBar = cr.ui.define('div');

  HeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether guest button should be shown when header bar is in normal mode.
    showGuest_: true,

    // Current UI state of the sign-in screen.
    signinUIState_: SIGNIN_UI_STATE.ACCOUNT_PICKER,

    // Whether to show kiosk apps menu.
    hasApps_: false,

    /** @override */
    decorate: function() {
      $('add-user-button').addEventListener('click',
          this.handleAddUserClick_);
      $('cancel-add-user-button').addEventListener('click',
          this.handleCancelAddUserClick_);
      $('guest-user-header-bar-item').addEventListener('click',
          this.handleGuestClick_);
      $('guest-user-button').addEventListener('click',
          this.handleGuestClick_);
      this.updateUI_();
    },

    /**
     * Tab index value for all button elements.
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
     * @private
     */
    handleAddUserClick_: function(e) {
      chrome.send('addUser');
      // Prevent further propagation of click event. Otherwise, the click event
      // handler of document object will set wallpaper to user's wallpaper when
      // there is only one existing user. See http://crbug.com/166477
      e.stopPropagation();
    },

    /**
     * Cancel add user button click handler.
     * @private
     */
    handleCancelAddUserClick_: function(e) {
      // Let screen handle cancel itself if that is capable of doing so.
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.cancel) {
        Oobe.getInstance().currentScreen.cancel();
        return;
      }

      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
      Oobe.resetSigninUI(true);
    },

    /**
     * Guest button click handler.
     * @private
     */
    handleGuestClick_: function(e) {
      chrome.send('launchGuest');
      e.stopPropagation();
    },

    /**
     * If true then "Browse as Guest" button is shown.
     * @type {boolean}
     */
    set showGuestButton(value) {
      this.showGuest_ = value;
      this.updateUI_();
    },

    /**
     * Update current header bar UI.
     * @type {number} state Current state of the sign-in screen
     *                      (see SIGNIN_UI_STATE).
     */
    set signinUIState(state) {
      this.signinUIState_ = state;
      this.updateUI_();
    },

    /**
     * Whether the Cancel button is enabled during Gaia sign-in.
     * @type {boolean}
     */
    set allowCancel(value) {
      this.allowCancel_ = value;
      this.updateUI_();
    },

    /**
     * Updates visibility state of action buttons.
     * @private
     */
    updateUI_: function() {
      $('add-user-button').hidden = false;
      $('cancel-add-user-button').hidden = !this.allowCancel_;
      $('guest-user-header-bar-item').hidden = false;
      $('add-user-header-bar-item').hidden = false;
    },

    /**
     * Animates Header bar to hide from the screen.
     * @param {function()} callback will be called once animation is finished.
     */
    animateOut: function(callback) {
      var launcher = this;
      launcher.addEventListener(
          'webkitTransitionEnd', function f(e) {
            launcher.removeEventListener('webkitTransitionEnd', f);
            callback();
          });
      this.classList.remove('login-header-bar-animate-slow');
      this.classList.add('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-hidden');
    },

    /**
     * Animates Header bar to slowly appear on the screen.
     */
    animateIn: function() {
      this.classList.remove('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-animate-slow');
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
  HeaderBar.animateIn = function() {
    $('login-header-bar').animateIn();
  }

  return {
    HeaderBar: HeaderBar
  };
});
