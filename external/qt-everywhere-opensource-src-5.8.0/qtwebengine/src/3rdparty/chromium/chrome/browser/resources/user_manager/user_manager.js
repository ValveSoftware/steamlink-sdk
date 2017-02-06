// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
<include src="../../../../ui/login/screen.js">
<include src="../../../../ui/login/bubble.js">
<include src="../../../../ui/login/login_ui_tools.js">
<include src="../../../../ui/login/display_manager.js">
<include src="control_bar.js">
<include src="../../../../ui/login/account_picker/screen_account_picker.js">
<include src="../../../../ui/login/account_picker/user_pod_row.js">
<include src="../../../../ui/login/resource_loader.js">
<include src="user_manager_tutorial.js">

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;
  var UserManagerTutorial = cr.ui.login.UserManagerTutorial;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function UserManager() {
  }

  cr.addSingletonGetter(UserManager);

  UserManager.prototype = {
    __proto__: DisplayManager.prototype,

    /**
     * Indicates whether the user pods page is visible.
     * @type {boolean}
     */
    userPodsPageVisible: true
  };

  /**
   * Shows the given screen.
   * @param {bool} showGuest Whether the 'Browse as Guest' button is displayed.
   * @param {bool} showAddPerson Whether the 'Add Person' button is displayed.
   */
  UserManager.showUserManagerScreen = function(showGuest, showAddPerson) {
    UserManager.getInstance().showScreen({id: 'account-picker',
                                   data: {disableAddUser: false}});
    // The ChromeOS account-picker will hide the AddUser button if a user is
    // logged in and the screen is "locked", so we must re-enabled it
    $('add-user-header-bar-item').hidden = false;

    // Hide control options if the user does not have the right permissions.
    $('guest-user-button').hidden = !showGuest;
    $('add-user-button').hidden = !showAddPerson;
    $('login-header-bar').hidden = false;

    // Disable the context menu, as the Print/Inspect element items don't
    // make sense when displayed as a widget.
    document.addEventListener('contextmenu', function(e) {e.preventDefault();});

    var hash = window.location.hash;
    if (hash && hash == '#tutorial')
      UserManagerTutorial.startTutorial();
  };

  /**
   * Open a new browser for the given profile.
   * @param {string} profilePath The profile's path.
   */
  UserManager.launchUser = function(profilePath) {
    chrome.send('launchUser', [profilePath]);
  };

  /**
   * Disables signin UI.
   */
  UserManager.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  UserManager.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  UserManager.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  UserManager.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  /**
   * Clears password field in user-pod.
   */
  UserManager.clearUserPodPassword = function() {
    DisplayManager.clearUserPodPassword();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  UserManager.refocusCurrentPod = function() {
    DisplayManager.refocusCurrentPod();
  };

  /**
   * Show the user manager tutorial
   * @param {string} email The user's email, if signed in.
   * @param {string} displayName The user's display name.
   */
  UserManager.showUserManagerTutorial = function() {
    UserManagerTutorial.startTutorial();
  };

  // Export
  return {
    UserManager: UserManager
  };
});

cr.define('UserManager', function() {
  'use strict';

  function initialize() {
    cr.ui.login.DisplayManager.initialize();
    cr.ui.login.UserManagerTutorial.initialize();
    login.AccountPickerScreen.register();
    cr.ui.Bubble.decorate($('bubble'));
    login.HeaderBar.decorate($('login-header-bar'));

    // Hide the header bar until the showUserManagerMethod can apply function
    // parameters that affect widget visiblity.
    $('login-header-bar').hidden = true;

    chrome.send('userManagerInitialize', [window.location.hash]);
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize
  };
});

// Alias to Oobe for use in src/ui/login/account_picker/user_pod_row.js
var Oobe = cr.ui.UserManager;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', UserManager.initialize);
