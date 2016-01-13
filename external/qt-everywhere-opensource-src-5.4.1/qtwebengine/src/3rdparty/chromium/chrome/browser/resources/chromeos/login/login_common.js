// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common OOBE controller methods.
 */

<include src="../../login/screen.js"></include>
<include src="screen_context.js"></include>
<include src="../user_images_grid.js"></include>
<include src="apps_menu.js"></include>
<include src="../../login/bubble.js"></include>
<include src="../../login/display_manager.js"></include>
<include src="header_bar.js"></include>
<include src="network_dropdown.js"></include>
<include src="oobe_screen_reset.js"></include>
<include src="oobe_screen_autolaunch.js"></include>
<include src="oobe_screen_enable_kiosk.js"></include>
<include src="oobe_screen_terms_of_service.js"></include>
<include src="oobe_screen_user_image.js"></include>
<include src="../../login/screen_account_picker.js"></include>
<include src="screen_app_launch_splash.js"></include>
<include src="screen_error_message.js"></include>
<include src="screen_gaia_signin.js"></include>
<include src="screen_locally_managed_user_creation.js"></include>
<include src="screen_password_changed.js"></include>
<include src="screen_tpm_error.js"></include>
<include src="screen_wrong_hwid.js"></include>
<include src="screen_confirm_password.js"></include>
<include src="screen_fatal_error.js"></include>
<include src="../../login/user_pod_row.js"></include>
<include src="../../login/resource_loader.js"></include>

cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function Oobe() {
  }

  /**
   * Delay in milliseconds between start of OOBE animation and start of
   * header bar animation.
   */
  var HEADER_BAR_DELAY_MS = 300;

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Handle accelerators. These are passed from native code instead of a JS
   * event handler in order to make sure that embedded iframes cannot swallow
   * them.
   * @param {string} name Accelerator name.
   */
  Oobe.handleAccelerator = function(name) {
    Oobe.getInstance().handleAccelerator(name);
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showScreen = function(screen) {
    Oobe.getInstance().showScreen(screen);
  };

  /**
   * Updates version label visibilty.
   * @param {boolean} show True if version label should be visible.
   */
  Oobe.showVersion = function(show) {
    Oobe.getInstance().showVersion(show);
  };

  /**
   * Update body class to switch between OOBE UI and Login UI.
   */
  Oobe.showOobeUI = function(showOobe) {
    if (showOobe) {
      document.body.classList.add('oobe-display');

      // Callback to animate the header bar in.
      var showHeaderBar = function() {
        login.HeaderBar.animateIn(function() {
          chrome.send('headerBarVisible');
        });
      };
      // Start asynchronously so the OOBE network screen comes in first.
      window.setTimeout(showHeaderBar, HEADER_BAR_DELAY_MS);
    } else {
      document.body.classList.remove('oobe-display');
      Oobe.getInstance().prepareForLoginDisplay_();
    }

    Oobe.getInstance().headerHidden = false;
  };

  /**
   * Enables keyboard driven flow.
   */
  Oobe.enableKeyboardFlow = function(value) {
    // Don't show header bar for OOBE.
    Oobe.getInstance().forceKeyboardFlow = value;
  };

  /**
   * Disables signin UI.
   */
  Oobe.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   * If |forceOnline| is false previously used sign-in type will be used.
   */
  Oobe.resetSigninUI = function(forceOnline) {
    DisplayManager.resetSigninUI(forceOnline);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  Oobe.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Shows password changed screen that offers migration.
   * @param {boolean} showError Whether to show the incorrect password error.
   */
  Oobe.showPasswordChangedScreen = function(showError) {
    DisplayManager.showPasswordChangedScreen(showError);
  };

  /**
   * Shows dialog to create managed user.
   */
  Oobe.showManagedUserCreationScreen = function() {
    DisplayManager.showManagedUserCreationScreen();
  };

  /**
   * Shows TPM error screen.
   */
  Oobe.showTpmError = function() {
    DisplayManager.showTpmError();
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  Oobe.clearErrors = function() {
    var accessibilityMenu = $('accessibility-menu');
    if (accessibilityMenu)
      accessibilityMenu.hide();
    DisplayManager.clearErrors();
  };

  /**
   * Displays animations on successful authentication, that have to happen
   * before login UI is dismissed.
   */
  Oobe.animateAuthenticationSuccess = function() {
    login.HeaderBar.animateOut(function() {
      chrome.send('unlockOnLoginSuccess');
    });
  };

  /**
   * Displays animations that have to happen once login UI is fully displayed.
   */
  Oobe.animateOnceFullyDisplayed = function() {
    login.HeaderBar.animateIn();
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  Oobe.setLabelText = function(labelId, labelText) {
    DisplayManager.setLabelText(labelId, labelText);
  };

  /**
   * Sets the text content of the enterprise info message.
   * If the text is empty, the entire notification will be hidden.
   * @param {string} messageText The message text.
   */
  Oobe.setEnterpriseInfo = function(messageText) {
    DisplayManager.setEnterpriseInfo(messageText);
  };

  /**
   * Updates the device requisition string shown in the requisition prompt.
   * @param {string} requisition The device requisition.
   */
  Oobe.updateDeviceRequisition = function(requisition) {
    Oobe.getInstance().updateDeviceRequisition(requisition);
  };

  /**
   * Enforces focus on user pod of locked user.
   */
  Oobe.forceLockedUserPodFocus = function() {
    login.AccountPickerScreen.forceLockedUserPodFocus();
  };

  /**
   * Clears password field in user-pod.
   */
  Oobe.clearUserPodPassword = function() {
    DisplayManager.clearUserPodPassword();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  Oobe.refocusCurrentPod = function() {
    DisplayManager.refocusCurrentPod();
  };

  /**
   * Skip to login screen for telemetry.
   */
  Oobe.skipToLoginForTesting = function() {
    Oobe.disableSigninUI();
    chrome.send('skipToLoginForTesting');
  };

  /**
   * Login for telemetry.
   * @param {string} username Login username.
   * @param {string} password Login password.
   */
  Oobe.loginForTesting = function(username, password) {
    Oobe.disableSigninUI();
    chrome.send('skipToLoginForTesting', [username]);
    chrome.send('completeLogin', [username, password, false]);
  };

  /**
   * Guest login for telemetry.
   */
  Oobe.guestLoginForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('launchIncognito');
  };

  /**
   * Authenticate for telemetry - used for screenlocker.
   * @param {string} username Login username.
   * @param {string} password Login password.
   */
  Oobe.authenticateForTesting = function(username, password) {
    Oobe.disableSigninUI();
    chrome.send('authenticateUser', [username, password]);
  };

  /**
   * Gaia login screen for telemetry.
   */
  Oobe.addUserForTesting = function() {
    Oobe.skipToLoginForTesting();
    chrome.send('addUser');
  };

  /**
   * Hotrod requisition for telemetry.
   */
  Oobe.remoraRequisitionForTesting = function() {
    chrome.send('setDeviceRequisition', ['remora']);
  };

  /**
   * Finish enterprise enrollment for telemetry.
   */
  Oobe.enterpriseEnrollmentDone = function() {
    chrome.send('oauthEnrollClose', ['done']);
  };

  /**
   * Shows/hides login UI control bar with buttons like [Shut down].
   */
  Oobe.showControlBar = function(show) {
    Oobe.getInstance().headerHidden = !show;
  };

  /**
   * Sets the current state of the virtual keyboard (shown/hidden, size).
   */
  Oobe.setKeyboardState = function(shown, width, height) {
    Oobe.getInstance().virtualKeyboardShown = shown;
    Oobe.getInstance().setVirtualKeyboardSize(width, height);
  };

  /**
   * Sets the current size of the client area (display size).
   * @param {number} width client area width
   * @param {number} height client area height
   */
  Oobe.setClientAreaSize = function(width, height) {
    Oobe.getInstance().setClientAreaSize(width, height);
  };

  // Export
  return {
    Oobe: Oobe
  };
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

// Register assets for async loading.
[{
  id: SCREEN_OOBE_ENROLLMENT,
  html: [{ url: 'chrome://oobe/enrollment.html', targetID: 'inner-container' }],
  css: ['chrome://oobe/enrollment.css'],
  js: ['chrome://oobe/enrollment.js']
}].forEach(cr.ui.login.ResourceLoader.registerAssets);

document.addEventListener('DOMContentLoaded', function() {
  'use strict';

  // Immediately load async assets.
  // TODO(dconnelly): remove this at some point and only load as needed.
  // See crbug.com/236426
  cr.ui.login.ResourceLoader.loadAssets(SCREEN_OOBE_ENROLLMENT, function() {
    // This screen is async-loaded so we manually trigger i18n processing.
    i18nTemplate.process($('oauth-enrollment'), loadTimeData);
    // Delayed binding since this isn't defined yet.
    login.OAuthEnrollmentScreen.register();
  });

  // Delayed binding since this isn't defined yet.
  cr.ui.Oobe.initialize();
});
