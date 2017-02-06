// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Account picker screen implementation.
 */

login.createScreen('AccountPickerScreen', 'account-picker', function() {
  /**
   * Maximum number of offline login failures before online login.
   * @type {number}
   * @const
   */
  var MAX_LOGIN_ATTEMPTS_IN_POD = 3;

  return {
    EXTERNAL_API: [
      'loadUsers',
      'runAppForTesting',
      'setApps',
      'setShouldShowApps',
      'showAppError',
      'updateUserImage',
      'setCapsLockState',
      'forceLockedUserPodFocus',
      'removeUser',
      'showBannerMessage',
      'showUserPodCustomIcon',
      'hideUserPodCustomIcon',
      'disablePinKeyboardForUser',
      'setAuthType',
      'setTouchViewState',
      'setPublicSessionDisplayName',
      'setPublicSessionLocales',
      'setPublicSessionKeyboardLayouts',
    ],

    preferredWidth_: 0,
    preferredHeight_: 0,

    // Whether this screen is shown for the first time.
    firstShown_: true,

    // Whether this screen is currently being shown.
    showing_: false,

    /** @override */
    decorate: function() {
      login.PodRow.decorate($('pod-row'));
    },

    /** @override */
    getPreferredSize: function() {
      return {width: this.preferredWidth_, height: this.preferredHeight_};
    },

    /** @override */
    onWindowResize: function() {
      $('pod-row').onWindowResize();

      // Reposition the error bubble, if it is showing. Since we are just
      // moving the bubble, the number of login attempts tried doesn't matter.
      var errorBubble = $('bubble');
      if (errorBubble && !errorBubble.hidden)
        this.showErrorBubble(0, undefined  /* Reuses the existing message. */);
    },

    /**
     * Sets preferred size for account picker screen.
     */
    setPreferredSize: function(width, height) {
      this.preferredWidth_ = width;
      this.preferredHeight_ = height;
    },

    /**
     * When the account picker is being used to lock the screen, pressing the
     * exit accelerator key will sign out the active user as it would when
     * they are signed in.
     */
    exit: function() {
      // Check and disable the sign out button so that we can never have two
      // sign out requests generated in a row.
      if ($('pod-row').lockedPod && !$('sign-out-user-button').disabled) {
        $('sign-out-user-button').disabled = true;
        chrome.send('signOutUser');
      }
    },

    /* Cancel user adding if ESC was pressed.
     */
    cancel: function() {
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.USER_ADDING)
        chrome.send('cancelUserAdding');
    },

    /**
     * Event handler that is invoked just after the frame is shown.
     * @param {string} data Screen init payload.
     */
    onAfterShow: function(data) {
      $('pod-row').handleAfterShow();
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      this.showing_ = true;
      chrome.send('loginUIStateChanged', ['account-picker', true]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
      chrome.send('hideCaptivePortal');
      var podRow = $('pod-row');
      podRow.handleBeforeShow();

      // In case of the preselected pod onShow will be called once pod
      // receives focus.
      if (!podRow.preselectedPod)
        this.onShow();
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onShow: function() {
      if (!this.showing_) {
        // This method may be called asynchronously when the pod row finishes
        // initializing. However, at that point, the screen may have been hidden
        // again already. If that happens, ignore the onShow() call.
        return;
      }
      chrome.send('getTouchViewState');
      if (!this.firstShown_) return;
      this.firstShown_ = false;

      // Ensure that login is actually visible.
      window.requestAnimationFrame(function() {
        chrome.send('accountPickerReady');
        chrome.send('loginVisible', ['account-picker']);
      });
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
      $('pod-row').clearFocusedPod();
      this.showing_ = false;
      chrome.send('loginUIStateChanged', ['account-picker', false]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
      $('pod-row').handleHide();
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} content Content to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      var activatedPod = $('pod-row').activatedPod;
      if (!activatedPod) {
        $('bubble').showContentForElement($('pod-row'),
                                          cr.ui.Bubble.Attachment.RIGHT,
                                          error);
        return;
      }
      // Show web authentication if this is not a supervised user.
      if (loginAttempts > MAX_LOGIN_ATTEMPTS_IN_POD &&
          !activatedPod.user.supervisedUser) {
        chrome.send('maxIncorrectPasswordAttempts',
            [activatedPod.user.emailAddress]);
        activatedPod.showSigninUI();
      } else {
        if (loginAttempts == 1) {
          chrome.send('firstIncorrectPasswordAttempt',
              [activatedPod.user.emailAddress]);
        }
        // We want bubble's arrow to point to the first letter of input.
        /** @const */ var BUBBLE_OFFSET = 7;
        /** @const */ var BUBBLE_PADDING = 4;
        $('bubble').showContentForElement(activatedPod.mainInput,
                                          cr.ui.Bubble.Attachment.BOTTOM,
                                          error,
                                          BUBBLE_OFFSET, BUBBLE_PADDING);
        // Move error bubble up if it overlaps the shelf.
        var maxHeight =
            cr.ui.LoginUITools.getMaxHeightBeforeShelfOverlapping($('bubble'));
        if (maxHeight < $('bubble').offsetHeight) {
          $('bubble').showContentForElement(activatedPod.mainInput,
                                            cr.ui.Bubble.Attachment.TOP,
                                            error,
                                            BUBBLE_OFFSET, BUBBLE_PADDING);
        }
      }
    },

    /**
     * Loads the PIN keyboard if any of the users can login with a PIN.
     * @param {array} users Array of user instances.
     */
    loadPinKeyboardIfNeeded_: function(users) {
      for (var i = 0; i < users.length; ++i) {
        var user = users[i];
        if (user.showPin) {
          showPinKeyboardAsync();
          return;
        }
      }
    },

    /**
     * Loads given users in pod row.
     * @param {array} users Array of user.
     * @param {boolean} showGuest Whether to show guest session button.
     */
    loadUsers: function(users, showGuest) {
      $('pod-row').loadPods(users);
      $('login-header-bar').showGuestButton = showGuest;
      // On Desktop, #login-header-bar has a shadow if there are 8+ profiles.
      if (Oobe.getInstance().displayType == DISPLAY_TYPE.DESKTOP_USER_MANAGER)
        $('login-header-bar').classList.toggle('shadow', users.length > 8);

      this.loadPinKeyboardIfNeeded_(users);
    },

    /**
     * Runs app with a given id from the list of loaded apps.
     * @param {!string} app_id of an app to run.
     * @param {boolean=} opt_diagnostic_mode Whether to run the app in
     *     diagnostic mode.  Default is false.
     */
    runAppForTesting: function(app_id, opt_diagnostic_mode) {
      $('pod-row').findAndRunAppForTesting(app_id, opt_diagnostic_mode);
    },

    /**
     * Adds given apps to the pod row.
     * @param {array} apps Array of apps.
     */
    setApps: function(apps) {
      $('pod-row').setApps(apps);
    },

    /**
     * Sets the flag of whether app pods should be visible.
     * @param {boolean} shouldShowApps Whether to show app pods.
     */
    setShouldShowApps: function(shouldShowApps) {
      $('pod-row').setShouldShowApps(shouldShowApps);
    },

    /**
     * Shows the given kiosk app error message.
     * @param {!string} message Error message to show.
     */
    showAppError: function(message) {
      // TODO(nkostylev): Figure out a way to show kiosk app launch error
      // pointing to the kiosk app pod.
      /** @const */ var BUBBLE_PADDING = 12;
      $('bubble').showTextForElement($('pod-row'),
                                     message,
                                     cr.ui.Bubble.Attachment.BOTTOM,
                                     $('pod-row').offsetWidth / 2,
                                     BUBBLE_PADDING);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      $('pod-row').updateUserImage(username);
    },

    /**
     * Updates Caps Lock state (for Caps Lock hint in password input field).
     * @param {boolean} enabled Whether Caps Lock is on.
     */
    setCapsLockState: function(enabled) {
      $('pod-row').classList.toggle('capslock-on', enabled);
    },

    /**
     * Enforces focus on user pod of locked user.
     */
    forceLockedUserPodFocus: function() {
      var row = $('pod-row');
      if (row.lockedPod)
        row.focusPod(row.lockedPod, true);
    },

    /**
     * Remove given user from pod row if it is there.
     * @param {string} user name.
     */
    removeUser: function(username) {
      $('pod-row').removeUserPod(username);
    },

    /**
     * Displays a banner containing |message|. If the banner is already present
     * this function updates the message in the banner. This function is used
     * by the chrome.screenlockPrivate.showMessage API.
     * @param {string} message Text to be displayed
     */
    showBannerMessage: function(message) {
      var banner = $('signin-banner');
      banner.textContent = message;
      banner.classList.toggle('message-set', true);
    },

    /**
     * Shows a custom icon in the user pod of |username|. This function
     * is used by the chrome.screenlockPrivate API.
     * @param {string} username Username of pod to add button
     * @param {!{id: !string,
     *           hardlockOnClick: boolean,
     *           isTrialRun: boolean,
     *           tooltip: ({text: string, autoshow: boolean} | undefined)}} icon
     *     The icon parameters.
     */
    showUserPodCustomIcon: function(username, icon) {
      $('pod-row').showUserPodCustomIcon(username, icon);
    },

    /**
     * Hides the custom icon in the user pod of |username| added by
     * showUserPodCustomIcon(). This function is used by the
     * chrome.screenlockPrivate API.
     * @param {string} username Username of pod to remove button
     */
    hideUserPodCustomIcon: function(username) {
      $('pod-row').hideUserPodCustomIcon(username);
    },

    /**
     * Sets the authentication type used to authenticate the user.
     * @param {string} username Username of selected user
     * @param {number} authType Authentication type, must be a valid value in
     *                          the AUTH_TYPE enum in user_pod_row.js.
     * @param {string} value The initial value to use for authentication.
     */
    setAuthType: function(username, authType, value) {
      $('pod-row').setAuthType(username, authType, value);
    },

    /**
     * Sets the state of touch view mode.
     * @param {boolean} isTouchViewEnabled true if the mode is on.
     */
    setTouchViewState: function(isTouchViewEnabled) {
      $('pod-row').setTouchViewState(isTouchViewEnabled);
    },

    /**
     * Hides the PIN keyboard if it's active.
     * @param {!user} user The user who can no longer enter a PIN.
     */
    disablePinKeyboardForUser: function(user) {
      $('pod-row').setPinVisibility(user, false);
    },

    /**
     * Updates the display name shown on a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} displayName The new display name
     */
    setPublicSessionDisplayName: function(userID, displayName) {
      $('pod-row').setPublicSessionDisplayName(userID, displayName);
    },

    /**
     * Updates the list of locales available for a public session.
     * @param {string} userID The user ID of the public session
     * @param {!Object} locales The list of available locales
     * @param {string} defaultLocale The locale to select by default
     * @param {boolean} multipleRecommendedLocales Whether |locales| contains
     *     two or more recommended locales
     */
    setPublicSessionLocales: function(userID,
                                      locales,
                                      defaultLocale,
                                      multipleRecommendedLocales) {
      $('pod-row').setPublicSessionLocales(userID,
                                           locales,
                                           defaultLocale,
                                           multipleRecommendedLocales);
    },

    /**
     * Updates the list of available keyboard layouts for a public session pod.
     * @param {string} userID The user ID of the public session
     * @param {string} locale The locale to which this list of keyboard layouts
     *     applies
     * @param {!Object} list List of available keyboard layouts
     */
    setPublicSessionKeyboardLayouts: function(userID, locale, list) {
      $('pod-row').setPublicSessionKeyboardLayouts(userID, locale, list);
    }
  };
});
