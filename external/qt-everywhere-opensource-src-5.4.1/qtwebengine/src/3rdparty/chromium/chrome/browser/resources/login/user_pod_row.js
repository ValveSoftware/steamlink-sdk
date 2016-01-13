// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Number of displayed columns depending on user pod count.
   * @type {Array.<number>}
   * @const
   */
  var COLUMNS = [0, 1, 2, 3, 4, 5, 4, 4, 4, 5, 5, 6, 6, 5, 5, 6, 6, 6, 6];

  /**
   * Mapping between number of columns in pod-row and margin between user pods
   * for such layout.
   * @type {Array.<number>}
   * @const
   */
  var MARGIN_BY_COLUMNS = [undefined, 40, 40, 40, 40, 40, 12];

  /**
   * Mapping between number of columns in the desktop pod-row and margin
   * between user pods for such layout.
   * @type {Array.<number>}
   * @const
   */
  var DESKTOP_MARGIN_BY_COLUMNS = [undefined, 15, 15, 15, 15, 15, 15];

  /**
   * Maximal number of columns currently supported by pod-row.
   * @type {number}
   * @const
   */
  var MAX_NUMBER_OF_COLUMNS = 6;

  /**
   * Maximal number of rows if sign-in banner is displayed alonside.
   * @type {number}
   * @const
   */
  var MAX_NUMBER_OF_ROWS_UNDER_SIGNIN_BANNER = 2;

  /**
   * Variables used for pod placement processing. Width and height should be
   * synced with computed CSS sizes of pods.
   */
  var POD_WIDTH = 180;
  var PUBLIC_EXPANDED_WIDTH = 420;
  var CROS_POD_HEIGHT = 213;
  var DESKTOP_POD_HEIGHT = 216;
  var POD_ROW_PADDING = 10;
  var DESKTOP_ROW_PADDING = 15;

  /**
   * Minimal padding between user pod and virtual keyboard.
   * @type {number}
   * @const
   */
  var USER_POD_KEYBOARD_MIN_PADDING = 20;

  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

  /**
   * Maximum time for which the pod row remains hidden until all user images
   * have been loaded.
   * @type {number}
   * @const
   */
  var POD_ROW_IMAGES_LOAD_TIMEOUT_MS = 3000;

  /**
   * Public session help topic identifier.
   * @type {number}
   * @const
   */
  var HELP_TOPIC_PUBLIC_SESSION = 3041033;

  /**
   * Tab order for user pods. Update these when adding new controls.
   * @enum {number}
   * @const
   */
  var UserPodTabOrder = {
    POD_INPUT: 1,     // Password input fields (and whole pods themselves).
    HEADER_BAR: 2,    // Buttons on the header bar (Shutdown, Add User).
    ACTION_BOX: 3,    // Action box buttons.
    PAD_MENU_ITEM: 4  // User pad menu items (Remove this user).
  };

  /**
   * Supported authentication types. Keep in sync with the enum in
   * chrome/browser/chromeos/login/login_display.h
   * @enum {number}
   * @const
   */
  var AUTH_TYPE = {
    OFFLINE_PASSWORD: 0,
    ONLINE_SIGN_IN: 1,
    NUMERIC_PIN: 2,
    USER_CLICK: 3,
  };

  /**
   * Names of authentication types.
   */
  var AUTH_TYPE_NAMES = {
    0: 'offlinePassword',
    1: 'onlineSignIn',
    2: 'numericPin',
    3: 'userClick',
  };

  // Focus and tab order are organized as follows:
  //
  // (1) all user pods have tab index 1 so they are traversed first;
  // (2) when a user pod is activated, its tab index is set to -1 and its
  // main input field gets focus and tab index 1;
  // (3) buttons on the header bar have tab index 2 so they follow user pods;
  // (4) Action box buttons have tab index 3 and follow header bar buttons;
  // (5) lastly, focus jumps to the Status Area and back to user pods.
  //
  // 'Focus' event is handled by a capture handler for the whole document
  // and in some cases 'mousedown' event handlers are used instead of 'click'
  // handlers where it's necessary to prevent 'focus' event from being fired.

  /**
   * Helper function to remove a class from given element.
   * @param {!HTMLElement} el Element whose class list to change.
   * @param {string} cl Class to remove.
   */
  function removeClass(el, cl) {
    el.classList.remove(cl);
  }

  /**
   * Creates a user pod.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPod = cr.ui.define(function() {
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  /**
   * Stops event propagation from the any user pod child element.
   * @param {Event} e Event to handle.
   */
  function stopEventPropagation(e) {
    // Prevent default so that we don't trigger a 'focus' event.
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Unique salt added to user image URLs to prevent caching. Dictionary with
   * user names as keys.
   * @type {Object}
   */
  UserPod.userImageSalt_ = {};

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.actionBoxAreaElement.tabIndex = UserPodTabOrder.ACTION_BOX;

      this.addEventListener('keydown', this.handlePodKeyDown_.bind(this));
      this.addEventListener('click', this.handleClickOnPod_.bind(this));

      this.signinButtonElement.addEventListener('click',
          this.activate.bind(this));

      this.actionBoxAreaElement.addEventListener('mousedown',
                                                 stopEventPropagation);
      this.actionBoxAreaElement.addEventListener('click',
          this.handleActionAreaButtonClick_.bind(this));
      this.actionBoxAreaElement.addEventListener('keydown',
          this.handleActionAreaButtonKeyDown_.bind(this));

      this.actionBoxMenuRemoveElement.addEventListener('click',
          this.handleRemoveCommandClick_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('keydown',
          this.handleRemoveCommandKeyDown_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('blur',
          this.handleRemoveCommandBlur_.bind(this));

      if (this.actionBoxRemoveUserWarningButtonElement) {
        this.actionBoxRemoveUserWarningButtonElement.addEventListener(
            'click',
            this.handleRemoveUserConfirmationClick_.bind(this));
      }
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      this.passwordElement.addEventListener('keydown',
          this.parentNode.handleKeyDown.bind(this.parentNode));
      this.passwordElement.addEventListener('keypress',
          this.handlePasswordKeyPress_.bind(this));

      this.imageElement.addEventListener('load',
          this.parentNode.handlePodImageLoad.bind(this.parentNode, this));

      var initialAuthType = this.user.initialAuthType ||
          AUTH_TYPE.OFFLINE_PASSWORD;
      this.setAuthType(initialAuthType, null);
    },

    /**
     * Resets tab order for pod elements to its initial state.
     */
    resetTabOrder: function() {
      // Note: the |mainInput| can be the pod itself.
      this.mainInput.tabIndex = -1;
      this.tabIndex = UserPodTabOrder.POD_INPUT;
    },

    /**
     * Handles keypress event (i.e. any textual input) on password input.
     * @param {Event} e Keypress Event object.
     * @private
     */
    handlePasswordKeyPress_: function(e) {
      // When tabbing from the system tray a tab key press is received. Suppress
      // this so as not to type a tab character into the password field.
      if (e.keyCode == 9) {
        e.preventDefault();
        return;
      }
    },

    /**
     * Top edge margin number of pixels.
     * @type {?number}
     */
    set top(top) {
      this.style.top = cr.ui.toCssPx(top);
    },

    /**
     * Top edge margin number of pixels.
     */
    get top() {
      return parseInt(this.style.top);
    },

    /**
     * Left edge margin number of pixels.
     * @type {?number}
     */
    set left(left) {
      this.style.left = cr.ui.toCssPx(left);
    },

    /**
     * Left edge margin number of pixels.
     */
    get left() {
      return parseInt(this.style.left);
    },

    /**
     * Height number of pixels.
     */
    get height() {
      return this.offsetHeight;
    },

    /**
     * Gets signed in indicator element.
     * @type {!HTMLDivElement}
     */
    get signedInIndicatorElement() {
      return this.querySelector('.signed-in-indicator');
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.querySelector('.user-image');
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.querySelector('.name');
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.querySelector('.password');
    },

    /**
     * Gets the password label, which is used to show a message where the
     * password field is normally.
     * @type {!HTMLInputElement}
     */
    get passwordLabelElement() {
      return this.querySelector('.password-label');
    },

    /**
     * Gets Caps Lock hint image.
     * @type {!HTMLImageElement}
     */
    get capslockHintElement() {
      return this.querySelector('.capslock-hint');
    },

    /**
     * Gets user sign in button.
     * @type {!HTMLButtonElement}
     */
    get signinButtonElement() {
      return this.querySelector('.signin-button');
    },

    /**
     * Gets launch app button.
     * @type {!HTMLButtonElement}
     */
    get launchAppButtonElement() {
      return this.querySelector('.launch-app-button');
    },

    /**
     * Gets action box area.
     * @type {!HTMLInputElement}
     */
    get actionBoxAreaElement() {
      return this.querySelector('.action-box-area');
    },

    /**
     * Gets user type icon area.
     * @type {!HTMLDivElement}
     */
    get userTypeIconAreaElement() {
      return this.querySelector('.user-type-icon-area');
    },

    /**
     * Gets user type bubble like multi-profiles policy restriction message.
     * @type {!HTMLDivElement}
     */
    get userTypeBubbleElement() {
      return this.querySelector('.user-type-bubble');
    },

    /**
     * Gets user type icon.
     * @type {!HTMLDivElement}
     */
    get userTypeIconElement() {
      return this.querySelector('.user-type-icon-image');
    },

    /**
     * Gets action box menu.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuElement() {
      return this.querySelector('.action-box-menu');
    },

    /**
     * Gets action box menu title.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleElement() {
      return this.querySelector('.action-box-menu-title');
    },

    /**
     * Gets action box menu title, user name item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleNameElement() {
      return this.querySelector('.action-box-menu-title-name');
    },

    /**
     * Gets action box menu title, user email item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleEmailElement() {
      return this.querySelector('.action-box-menu-title-email');
    },

    /**
     * Gets action box menu, remove user command item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuCommandElement() {
      return this.querySelector('.action-box-menu-remove-command');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuRemoveElement() {
      return this.querySelector('.action-box-menu-remove');
    },

    /**
     * Gets action box menu, remove user warning text div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningTextElement() {
      return this.querySelector('.action-box-remove-user-warning-text');
    },

    /**
     * Gets action box menu, remove supervised user warning text div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveSupervisedUserWarningTextElement() {
      return this.querySelector(
          '.action-box-remove-supervised-user-warning-text');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningElement() {
      return this.querySelector('.action-box-remove-user-warning');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningButtonElement() {
      return this.querySelector('.remove-warning-button');
    },

    /**
     * Gets the locked user indicator box.
     * @type {!HTMLInputElement}
     */
    get lockedIndicatorElement() {
      return this.querySelector('.locked-indicator');
    },

    /**
     * Gets the supervised user indicator box.
     * @type {!HTMLInputElement}
     */
    get supervisedUserIndicatorElement() {
      return this.querySelector('.supervised-indicator');
    },

    /**
     * Gets the custom icon. This icon is normally hidden, but can be shown
     * using the chrome.screenlockPrivate API.
     * @type {!HTMLDivElement}
     */
    get customIconElement() {
      return this.querySelector('.custom-icon');
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      this.imageElement.src = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];

      this.nameElement.textContent = this.user_.displayName;
      this.signedInIndicatorElement.hidden = !this.user_.signedIn;

      this.signinButtonElement.hidden = !this.isAuthTypeOnlineSignIn;
      if (this.isAuthTypeUserClick)
        this.passwordLabelElement.textContent = this.authValue;

      this.updateActionBoxArea();

      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
        'passwordFieldAccessibleName', this.user_.emailAddress));

      this.customizeUserPodPerUserType();
    },

    updateActionBoxArea: function() {
      if (this.user_.publicAccount || this.user_.isApp) {
        this.actionBoxAreaElement.hidden = true;
        return;
      }

      this.actionBoxAreaElement.hidden = false;
      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;

      this.actionBoxAreaElement.setAttribute(
          'aria-label', loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label', loadTimeData.getString(
               'podMenuRemoveItemAccessibleName'));
      this.actionBoxMenuTitleNameElement.textContent = this.user_.isOwner ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;
      this.actionBoxMenuTitleEmailElement.hidden =
          this.user_.locallyManagedUser;

      this.actionBoxMenuCommandElement.textContent =
          loadTimeData.getString('removeUser');
    },

    customizeUserPodPerUserType: function() {
      if (this.user_.locallyManagedUser && !this.user_.isDesktopUser) {
        this.setUserPodIconType('supervised');
      } else if (this.multiProfilesPolicyApplied) {
        // Mark user pod as not focusable which in addition to the grayed out
        // filter makes it look in disabled state.
        this.classList.add('not-focusable');
        this.setUserPodIconType('policy');

        this.querySelector('.mp-policy-title').hidden = false;
        if (this.user.multiProfilesPolicy == 'primary-only')
          this.querySelector('.mp-policy-primary-only-msg').hidden = false;
        else if (this.user.multiProfilesPolicy == 'owner-primary-only')
          this.querySelector('.mp-owner-primary-only-msg').hidden = false;
        else
          this.querySelector('.mp-policy-not-allowed-msg').hidden = false;
      } else if (this.user_.isApp) {
        this.setUserPodIconType('app');
      }
    },

    setUserPodIconType: function(userTypeClass) {
      this.userTypeIconAreaElement.classList.add(userTypeClass);
      this.userTypeIconAreaElement.hidden = false;
    },

    /**
     * The user that this pod represents.
     * @type {!Object}
     */
    user_: undefined,
    get user() {
      return this.user_;
    },
    set user(userDict) {
      this.user_ = userDict;
      this.update();
    },

    /**
     * Returns true if multi-profiles sign in is currently active and this
     * user pod is restricted per policy.
     * @type {boolean}
     */
    get multiProfilesPolicyApplied() {
      var isMultiProfilesUI =
        (Oobe.getInstance().displayType == DISPLAY_TYPE.USER_ADDING);
      return isMultiProfilesUI && !this.user_.isMultiProfilesAllowed;
    },

    /**
     * Gets main input element.
     * @type {(HTMLButtonElement|HTMLInputElement)}
     */
    get mainInput() {
      if (this.isAuthTypePassword) {
        return this.passwordElement;
      } else if (this.isAuthTypeOnlineSignIn) {
        return this.signinButtonElement;
      } else if (this.isAuthTypeUserClick) {
        return this;
      }
    },

    /**
     * Whether action box button is in active state.
     * @type {boolean}
     */
    get isActionBoxMenuActive() {
      return this.actionBoxAreaElement.classList.contains('active');
    },
    set isActionBoxMenuActive(active) {
      if (active == this.isActionBoxMenuActive)
        return;

      if (active) {
        this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;
        if (this.actionBoxRemoveUserWarningElement)
          this.actionBoxRemoveUserWarningElement.hidden = true;

        // Clear focus first if another pod is focused.
        if (!this.parentNode.isFocused(this)) {
          this.parentNode.focusPod(undefined, true);
          this.actionBoxAreaElement.focus();
        }
        this.actionBoxAreaElement.classList.add('active');
      } else {
        this.actionBoxAreaElement.classList.remove('active');
      }
    },

    /**
     * Whether action box button is in hovered state.
     * @type {boolean}
     */
    get isActionBoxMenuHovered() {
      return this.actionBoxAreaElement.classList.contains('hovered');
    },
    set isActionBoxMenuHovered(hovered) {
      if (hovered == this.isActionBoxMenuHovered)
        return;

      if (hovered) {
        this.actionBoxAreaElement.classList.add('hovered');
        this.classList.add('hovered');
      } else {
        if (this.multiProfilesPolicyApplied)
          this.userTypeBubbleElement.classList.remove('bubble-shown');
        this.actionBoxAreaElement.classList.remove('hovered');
        this.classList.remove('hovered');
      }
    },

    /**
     * Set the authentication type for the pod.
     * @param {number} An auth type value defined in the AUTH_TYPE enum.
     * @param {string} authValue The initial value used for the auth type.
     */
    setAuthType: function(authType, authValue) {
      this.authType_ = authType;
      this.authValue_ = authValue;
      this.setAttribute('auth-type', AUTH_TYPE_NAMES[this.authType_]);
      this.update();
      this.reset(this.parentNode.isFocused(this));
    },

    /**
     * The auth type of the user pod. This value is one of the enum
     * values in AUTH_TYPE.
     * @type {number}
     */
    get authType() {
      return this.authType_;
    },

    /**
     * The initial value used for the pod's authentication type.
     * eg. a prepopulated password input when using password authentication.
     */
    get authValue() {
      return this.authValue_;
    },

    /**
     * True if the the user pod uses a password to authenticate.
     * @type {bool}
     */
    get isAuthTypePassword() {
      return this.authType_ == AUTH_TYPE.OFFLINE_PASSWORD;
    },

    /**
     * True if the the user pod uses a user click to authenticate.
     * @type {bool}
     */
    get isAuthTypeUserClick() {
      return this.authType_ == AUTH_TYPE.USER_CLICK;
    },

    /**
     * True if the the user pod uses a online sign in to authenticate.
     * @type {bool}
     */
    get isAuthTypeOnlineSignIn() {
      return this.authType_ == AUTH_TYPE.ONLINE_SIGN_IN;
    },

    /**
     * Updates the image element of the user.
     */
    updateUserImage: function() {
      UserPod.userImageSalt_[this.user.username] = new Date().getTime();
      this.update();
    },

    /**
     * Focuses on input element.
     */
    focusInput: function() {
      // Move tabIndex from the whole pod to the main input.
      // Note: the |mainInput| can be the pod itself.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /**
     * Activates the pod.
     * @param {Event} e Event object.
     * @return {boolean} True if activated successfully.
     */
    activate: function(e) {
      if (this.isAuthTypeOnlineSignIn) {
        this.showSigninUI();
      } else if (this.isAuthTypeUserClick) {
        Oobe.disableSigninUI();
        chrome.send('attemptUnlock', [this.user.username]);
      } else if (this.isAuthTypePassword) {
        if (!this.passwordElement.value)
          return false;
        Oobe.disableSigninUI();
        chrome.send('authenticateUser',
                    [this.user.username, this.passwordElement.value]);
      } else {
        console.error('Activating user pod with invalid authentication type: ' +
            this.authType);
      }

      return true;
    },

    showSupervisedUserSigninWarning: function() {
      // Locally managed user token has been invalidated.
      // Make sure that pod is focused i.e. "Sign in" button is seen.
      this.parentNode.focusPod(this);

      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent =
          loadTimeData.getString('supervisedUserExpiredTokenWarning');
      error.appendChild(messageDiv);

      $('bubble').showContentForElement(
          this.signinButtonElement,
          cr.ui.Bubble.Attachment.TOP,
          error,
          this.signinButtonElement.offsetWidth / 2,
          4);
    },

    /**
     * Shows signin UI for this user.
     */
    showSigninUI: function() {
      if (this.user.locallyManagedUser && !this.user.isDesktopUser) {
        this.showSupervisedUserSigninWarning();
      } else {
        // Special case for multi-profiles sign in. We show users even if they
        // are not allowed per policy. Restrict those users from starting GAIA.
        if (this.multiProfilesPolicyApplied)
          return;

        this.parentNode.showSigninUI(this.user.emailAddress);
      }
    },

    /**
     * Resets the input field and updates the tab order of pod controls.
     * @param {boolean} takeFocus If true, input field takes focus.
     */
    reset: function(takeFocus) {
      this.passwordElement.value = '';
      if (takeFocus)
        this.focusInput();  // This will set a custom tab order.
      else
        this.resetTabOrder();
    },

    /**
     * Handles a click event on action area button.
     * @param {Event} e Click event.
     */
    handleActionAreaButtonClick_: function(e) {
      if (this.parentNode.disabled)
        return;
      this.isActionBoxMenuActive = !this.isActionBoxMenuActive;
      e.stopPropagation();
    },

    /**
     * Handles a keydown event on action area button.
     * @param {Event} e KeyDown event.
     */
    handleActionAreaButtonKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
        case 'U+0020':  // Space
          if (this.parentNode.focusedPod_ && !this.isActionBoxMenuActive)
            this.isActionBoxMenuActive = true;
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          if (this.isActionBoxMenuActive) {
            this.actionBoxMenuRemoveElement.tabIndex =
                UserPodTabOrder.PAD_MENU_ITEM;
            this.actionBoxMenuRemoveElement.focus();
          }
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        case 'U+0009':  // Tab
          this.parentNode.focusPod();
        default:
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a click event on remove user command.
     * @param {Event} e Click event.
     */
    handleRemoveCommandClick_: function(e) {
      if (this.user.locallyManagedUser || this.user.isDesktopUser) {
        this.showRemoveWarning_();
        return;
      }
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Shows remove user warning. Used for supervised users on CrOS, and for all
     * users on desktop.
     */
    showRemoveWarning_: function() {
      this.actionBoxMenuRemoveElement.hidden = true;
      this.actionBoxRemoveUserWarningElement.hidden = false;
    },

    /**
     * Handles a click event on remove user confirmation button.
     * @param {Event} e Click event.
     */
    handleRemoveUserConfirmationClick_: function(e) {
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Handles a keydown event on remove command.
     * @param {Event} e KeyDown event.
     */
    handleRemoveCommandKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
          chrome.send('removeUser', [this.user.username]);
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        default:
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a blur event on remove command.
     * @param {Event} e Blur event.
     */
    handleRemoveCommandBlur_: function(e) {
      if (this.disabled)
        return;
      this.actionBoxMenuRemoveElement.tabIndex = -1;
    },

    /**
     * Handles click event on a user pod.
     * @param {Event} e Click event.
     */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      if (!this.isActionBoxMenuActive) {
        if (this.isAuthTypeOnlineSignIn) {
          this.showSigninUI();
        } else if (this.isAuthTypeUserClick) {
          this.parentNode.setActivatedPod(this);
        }

        if (this.multiProfilesPolicyApplied)
          this.userTypeBubbleElement.classList.add('bubble-shown');

        // Prevent default so that we don't trigger 'focus' event.
        e.preventDefault();
      }
    },

    /**
     * Handles keydown event for a user pod.
     * @param {Event} e Key event.
     */
    handlePodKeyDown_: function(e) {
      if (!this.isAuthTypeUserClick || this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
        case 'U+0020':  // Space
          if (this.parentNode.isFocused(this))
            this.parentNode.setActivatedPod(this);
          break;
      }
    }
  };

  /**
   * Creates a public account user pod.
   * @constructor
   * @extends {UserPod}
   */
  var PublicAccountUserPod = cr.ui.define(function() {
    var node = UserPod();

    var extras = $('public-account-user-pod-extras-template').children;
    for (var i = 0; i < extras.length; ++i) {
      var el = extras[i].cloneNode(true);
      node.appendChild(el);
    }

    return node;
  });

  PublicAccountUserPod.prototype = {
    __proto__: UserPod.prototype,

    /**
     * "Enter" button in expanded side pane.
     * @type {!HTMLButtonElement}
     */
    get enterButtonElement() {
      return this.querySelector('.enter-button');
    },

    /**
     * Boolean flag of whether the pod is showing the side pane. The flag
     * controls whether 'expanded' class is added to the pod's class list and
     * resets tab order because main input element changes when the 'expanded'
     * state changes.
     * @type {boolean}
     */
    get expanded() {
      return this.classList.contains('expanded');
    },

    /**
     * During transition final height of pod is not available because of
     * flexbox layout. That's why we have to calculate
     * the final height manually.
     */
    get expandedHeight_() {
      function getTopAndBottomPadding(domElement) {
        return parseInt(window.getComputedStyle(
            domElement).getPropertyValue('padding-top')) +
            parseInt(window.getComputedStyle(
                domElement).getPropertyValue('padding-bottom'));
      };
      var height =
        this.getElementsByClassName('side-pane-contents')[0].offsetHeight +
        this.getElementsByClassName('enter-button')[0].offsetHeight +
        getTopAndBottomPadding(
            this.getElementsByClassName('enter-button')[0]) +
        getTopAndBottomPadding(
            this.getElementsByClassName('side-pane-container')[0]) +
        getTopAndBottomPadding(this);
      return height;
    },

    set expanded(expanded) {
      if (this.expanded == expanded)
        return;

      this.resetTabOrder();
      this.classList.toggle('expanded', expanded);
      if (expanded) {
        var isDesktopUserManager = Oobe.getInstance().displayType ==
            DISPLAY_TYPE.DESKTOP_USER_MANAGER;
        var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                                POD_ROW_PADDING;
        this.usualLeft = this.left;
        this.usualTop = this.top;
        if (this.left + PUBLIC_EXPANDED_WIDTH >
            $('pod-row').offsetWidth - rowPadding)
          this.left = $('pod-row').offsetWidth - rowPadding -
              PUBLIC_EXPANDED_WIDTH;
        var expandedHeight = this.expandedHeight_;
        if (this.top + expandedHeight > $('pod-row').offsetHeight)
          this.top = $('pod-row').offsetHeight - expandedHeight;
      } else {
        if (typeof(this.usualLeft) != 'undefined')
          this.left = this.usualLeft;
        if (typeof(this.usualTop) != 'undefined')
          this.top = this.usualTop;
      }

      var self = this;
      this.classList.add('animating');
      this.addEventListener('webkitTransitionEnd', function f(e) {
        self.removeEventListener('webkitTransitionEnd', f);
        self.classList.remove('animating');

        // Accessibility focus indicator does not move with the focused
        // element. Sends a 'focus' event on the currently focused element
        // so that accessibility focus indicator updates its location.
        if (document.activeElement)
          document.activeElement.dispatchEvent(new Event('focus'));
      });
    },

    /** @override */
    get mainInput() {
      if (this.expanded)
        return this.enterButtonElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);

      this.classList.remove('need-password');
      this.classList.add('public-account');

      this.nameElement.addEventListener('keydown', (function(e) {
        if (e.keyIdentifier == 'Enter') {
          this.parentNode.setActivatedPod(this, e);
          // Stop this keydown event from bubbling up to PodRow handler.
          e.stopPropagation();
          // Prevent default so that we don't trigger a 'click' event on the
          // newly focused "Enter" button.
          e.preventDefault();
        }
      }).bind(this));

      var learnMore = this.querySelector('.learn-more');
      learnMore.addEventListener('mousedown', stopEventPropagation);
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      learnMore = this.querySelector('.side-pane-learn-more');
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      this.enterButtonElement.addEventListener('click', (function(e) {
        this.enterButtonElement.disabled = true;
        chrome.send('launchPublicAccount', [this.user.username]);
      }).bind(this));
    },

    /** @override **/
    update: function() {
      UserPod.prototype.update.call(this);
      this.querySelector('.side-pane-name').textContent =
          this.user_.displayName;
      this.querySelector('.info').textContent =
          loadTimeData.getStringF('publicAccountInfoFormat',
                                  this.user_.enterpriseDomain);
    },

    /** @override */
    focusInput: function() {
      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      if (!takeFocus)
        this.expanded = false;
      this.enterButtonElement.disabled = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function(e) {
      this.expanded = true;
      this.focusInput();
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      this.parentNode.focusPod(this);
      this.parentNode.setActivatedPod(this, e);
      // Prevent default so that we don't trigger 'focus' event.
      e.preventDefault();
    },

    /**
     * Handle mouse and keyboard events for the learn more button. Triggering
     * the button causes information about public sessions to be shown.
     * @param {Event} event Mouse or keyboard event.
     */
    handleLearnMoreEvent: function(event) {
      switch (event.type) {
        // Show informaton on left click. Let any other clicks propagate.
        case 'click':
          if (event.button != 0)
            return;
          break;
        // Show informaton when <Return> or <Space> is pressed. Let any other
        // key presses propagate.
        case 'keydown':
          switch (event.keyCode) {
            case 13:  // Return.
            case 32:  // Space.
              break;
            default:
              return;
          }
          break;
      }
      chrome.send('launchHelpApp', [HELP_TOPIC_PUBLIC_SESSION]);
      stopEventPropagation(event);
    },
  };

  /**
   * Creates a user pod to be used only in desktop chrome.
   * @constructor
   * @extends {UserPod}
   */
  var DesktopUserPod = cr.ui.define(function() {
    // Don't just instantiate a UserPod(), as this will call decorate() on the
    // parent object, and add duplicate event listeners.
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  DesktopUserPod.prototype = {
    __proto__: UserPod.prototype,

    /** @override */
    get mainInput() {
      if (!this.passwordElement.hidden)
        return this.passwordElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);
    },

    /** @override */
    update: function() {
      this.imageElement.src = this.user.userImage;
      this.nameElement.textContent = this.user.displayName;

      var isLockedUser = this.user.needsSignin;
      var isSupervisedUser = this.user.locallyManagedUser;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.supervisedUserIndicatorElement.hidden = !isSupervisedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      if (this.isAuthTypeUserClick)
        this.passwordLabelElement.textContent = this.authValue;

      this.actionBoxRemoveUserWarningTextElement.hidden = isSupervisedUser;
      this.actionBoxRemoveSupervisedUserWarningTextElement.hidden =
          !isSupervisedUser;

      UserPod.prototype.updateActionBoxArea.call(this);
    },

    /** @override */
    focusInput: function() {
      // For focused pods, display the name unless the pod is locked.
      var isLockedUser = this.user.needsSignin;
      var isSupervisedUser = this.user.locallyManagedUser;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.supervisedUserIndicatorElement.hidden = !isSupervisedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      // Always display the user's name for unfocused pods.
      if (!takeFocus)
        this.nameElement.hidden = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function(e) {
      if (this.passwordElement.hidden) {
        Oobe.launchUser(this.user.emailAddress, this.user.displayName);
      } else if (!this.passwordElement.value) {
        return false;
      } else {
        chrome.send('authenticatedLaunchUser',
                    [this.user.emailAddress,
                     this.user.displayName,
                     this.passwordElement.value]);
      }
      this.passwordElement.value = '';
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      Oobe.clearErrors();
      this.parentNode.lastFocusedPod_ = this;

      // If this is an unlocked pod, then open a browser window. Otherwise
      // just activate the pod and show the password field.
      if (!this.user.needsSignin && !this.isActionBoxMenuActive)
        this.activate(e);

      if (this.isAuthTypeUserClick)
        chrome.send('attemptUnlock', [this.user.emailAddress]);
    },

    /** @override */
    handleRemoveUserConfirmationClick_: function(e) {
      chrome.send('removeUser', [this.user.profilePath]);
    },
  };

  /**
   * Creates a user pod that represents kiosk app.
   * @constructor
   * @extends {UserPod}
   */
  var KioskAppPod = cr.ui.define(function() {
    var node = UserPod();
    return node;
  });

  KioskAppPod.prototype = {
    __proto__: UserPod.prototype,

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);
      this.launchAppButtonElement.addEventListener('click',
                                                   this.activate.bind(this));
    },

    /** @override */
    update: function() {
      this.imageElement.src = this.user.iconUrl;
      if (this.user.iconHeight && this.user.iconWidth) {
        this.imageElement.style.height = this.user.iconHeight;
        this.imageElement.style.width = this.user.iconWidth;
      }
      this.imageElement.alt = this.user.label;
      this.imageElement.title = this.user.label;
      this.passwordElement.hidden = true;
      this.signinButtonElement.hidden = true;
      this.launchAppButtonElement.hidden = false;
      this.signedInIndicatorElement.hidden = true;
      this.nameElement.textContent = this.user.label;

      UserPod.prototype.updateActionBoxArea.call(this);
      UserPod.prototype.customizeUserPodPerUserType.call(this);
    },

    /** @override */
    get mainInput() {
      return this.launchAppButtonElement;
    },

    /** @override */
    focusInput: function() {
      this.signinButtonElement.hidden = true;
      this.launchAppButtonElement.hidden = false;
      this.passwordElement.hidden = true;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    get forceOnlineSignin() {
      return false;
    },

    /** @override */
    activate: function(e) {
      var diagnosticMode = e && e.ctrlKey;
      this.launchApp_(this.user, diagnosticMode);
      return true;
    },

    /** @override */
    handleClickOnPod_: function(e) {
      if (this.parentNode.disabled)
        return;

      Oobe.clearErrors();
      this.parentNode.lastFocusedPod_ = this;
      this.activate(e);
    },

    /**
     * Launch the app. If |diagnosticMode| is true, ask user to confirm.
     * @param {Object} app App data.
     * @param {boolean} diagnosticMode Whether to run the app in diagnostic
     *     mode.
     */
    launchApp_: function(app, diagnosticMode) {
      if (!diagnosticMode) {
        chrome.send('launchKioskApp', [app.id, false]);
        return;
      }

      var oobe = $('oobe');
      if (!oobe.confirmDiagnosticMode_) {
        oobe.confirmDiagnosticMode_ =
            new cr.ui.dialogs.ConfirmDialog(document.body);
        oobe.confirmDiagnosticMode_.setOkLabel(
            loadTimeData.getString('confirmKioskAppDiagnosticModeYes'));
        oobe.confirmDiagnosticMode_.setCancelLabel(
            loadTimeData.getString('confirmKioskAppDiagnosticModeNo'));
      }

      oobe.confirmDiagnosticMode_.show(
          loadTimeData.getStringF('confirmKioskAppDiagnosticModeFormat',
                                  app.label),
          function() {
            chrome.send('launchKioskApp', [app.id, true]);
          });
    },
  };

  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether this user pod row is shown for the first time.
    firstShown_: true,

    // True if inside focusPod().
    insideFocusPod_: false,

    // Focused pod.
    focusedPod_: undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    // Pod that was most recently focused, if any.
    lastFocusedPod_: undefined,

    // Pods whose initial images haven't been loaded yet.
    podsWithPendingImages_: [],

    // Whether pod creation is animated.
    userAddIsAnimated_: false,

    // Whether pod placement has been postponed.
    podPlacementPostponed_: false,

    // Standard user pod height/width.
    userPodHeight_: 0,
    userPodWidth_: 0,

    // Array of apps that are shown in addition to other user pods.
    apps_: [],

    // True to show app pods along with user pods.
    shouldShowApps_: true,

    // Array of users that are shown (public/supervised/regular).
    users_: [],

    /** @override */
    decorate: function() {
      // Event listeners that are installed for the time period during which
      // the element is visible.
      this.listeners_ = {
        focus: [this.handleFocus_.bind(this), true /* useCapture */],
        click: [this.handleClick_.bind(this), true],
        mousemove: [this.handleMouseMove_.bind(this), false],
        keydown: [this.handleKeyDown.bind(this), false]
      };

      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      this.userPodHeight_ = isDesktopUserManager ? DESKTOP_POD_HEIGHT :
                                                   CROS_POD_HEIGHT;
      // Same for Chrome OS and desktop.
      this.userPodWidth_ = POD_WIDTH;
    },

    /**
     * Returns all the pods in this pod row.
     * @type {NodeList}
     */
    get pods() {
      return Array.prototype.slice.call(this.children);
    },

    /**
     * Return true if user pod row has only single user pod in it, which should
     * always be focused.
     * @type {boolean}
     */
    get alwaysFocusSinglePod() {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;

      return isDesktopUserManager ? false : this.children.length == 1;
    },

    /**
     * Returns pod with the given app id.
     * @param {!string} app_id Application id to be matched.
     * @return {Object} Pod with the given app id. null if pod hasn't been
     *     found.
     */
    getPodWithAppId_: function(app_id) {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.isApp && pod.user.id == app_id)
          return pod;
      }
      return null;
    },

    /**
     * Returns pod with the given username (null if there is no such pod).
     * @param {string} username Username to be matched.
     * @return {Object} Pod with the given username. null if pod hasn't been
     *     found.
     */
    getPodWithUsername_: function(username) {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.username == username)
          return pod;
      }
      return null;
    },

    /**
     * True if the the pod row is disabled (handles no user interaction).
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
    },

    /**
     * Creates a user pod from given email.
     * @param {!Object} user User info dictionary.
     */
    createUserPod: function(user) {
      var userPod;
      if (user.isDesktopUser)
        userPod = new DesktopUserPod({user: user});
      else if (user.publicAccount)
        userPod = new PublicAccountUserPod({user: user});
      else if (user.isApp)
        userPod = new KioskAppPod({user: user});
      else
        userPod = new UserPod({user: user});

      userPod.hidden = false;
      return userPod;
    },

    /**
     * Add an existing user pod to this pod row.
     * @param {!Object} user User info dictionary.
     * @param {boolean} animated Whether to use init animation.
     */
    addUserPod: function(user, animated) {
      var userPod = this.createUserPod(user);
      if (animated) {
        userPod.classList.add('init');
        userPod.nameElement.classList.add('init');
      }

      this.appendChild(userPod);
      userPod.initialize();
    },

    /**
     * Runs app with a given id from the list of loaded apps.
     * @param {!string} app_id of an app to run.
     * @param {boolean=} opt_diagnostic_mode Whether to run the app in
     *     diagnostic mode. Default is false.
     */
    findAndRunAppForTesting: function(app_id, opt_diagnostic_mode) {
      var app = this.getPodWithAppId_(app_id);
      if (app) {
        var activationEvent = cr.doc.createEvent('MouseEvents');
        var ctrlKey = opt_diagnostic_mode;
        activationEvent.initMouseEvent('click', true, true, null,
            0, 0, 0, 0, 0, ctrlKey, false, false, false, 0, null);
        app.dispatchEvent(activationEvent);
      }
    },

    /**
     * Removes user pod from pod row.
     * @param {string} email User's email.
     */
    removeUserPod: function(username) {
      var podToRemove = this.getPodWithUsername_(username);
      if (podToRemove == null) {
        console.warn('Attempt to remove not existing pod for ' + username +
            '.');
        return;
      }
      this.removeChild(podToRemove);
      if (this.pods.length > 0)
        this.placePods_();
    },

    /**
     * Returns index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    indexOf_: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        if (pod == this.pods[i])
          return i;
      }
      return -1;
    },

    /**
     * Start first time show animation.
     */
    startInitAnimation: function() {
      // Schedule init animation.
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        window.setTimeout(removeClass, 500 + i * 70, pod, 'init');
        window.setTimeout(removeClass, 700 + i * 70, pod.nameElement, 'init');
      }
    },

    /**
     * Start login success animation.
     */
    startAuthenticatedAnimation: function() {
      var activated = this.indexOf_(this.activatedPod_);
      if (activated == -1)
        return;

      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (i < activated)
          pod.classList.add('left');
        else if (i > activated)
          pod.classList.add('right');
        else
          pod.classList.add('zoom');
      }
    },

    /**
     * Populates pod row with given existing users and start init animation.
     * @param {array} users Array of existing user emails.
     * @param {boolean} animated Whether to use init animation.
     */
    loadPods: function(users, animated) {
      this.users_ = users;
      this.userAddIsAnimated_ = animated;

      this.rebuildPods();
    },

    /**
     * Scrolls focused user pod into view.
     */
    scrollFocusedPodIntoView: function() {
      var pod = this.focusedPod_;
      if (!pod)
        return;

      // First check whether focused pod is already fully visible.
      var visibleArea = $('scroll-container');
      var scrollTop = visibleArea.scrollTop;
      var clientHeight = visibleArea.clientHeight;
      var podTop = $('oobe').offsetTop + pod.offsetTop;
      var padding = USER_POD_KEYBOARD_MIN_PADDING;
      if (podTop + pod.height + padding <= scrollTop + clientHeight &&
          podTop - padding >= scrollTop) {
        return;
      }

      // Scroll so that user pod is as centered as possible.
      visibleArea.scrollTop = podTop - (clientHeight - pod.offsetHeight) / 2;
    },

    /**
     * Rebuilds pod row using users_ and apps_ that were previously set or
     * updated.
     */
    rebuildPods: function() {
      var emptyPodRow = this.pods.length == 0;

      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;
      this.lastFocusedPod_ = undefined;

      // Switch off animation
      Oobe.getInstance().toggleClass('flying-pods', false);

      // Populate the pod row.
      for (var i = 0; i < this.users_.length; ++i)
        this.addUserPod(this.users_[i], this.userAddIsAnimated_);

      for (var i = 0, pod; pod = this.pods[i]; ++i)
        this.podsWithPendingImages_.push(pod);

      // TODO(nkostylev): Edge case handling when kiosk apps are not fitting.
      if (this.shouldShowApps_) {
        for (var i = 0; i < this.apps_.length; ++i)
          this.addUserPod(this.apps_[i], this.userAddIsAnimated_);
      }

      // Make sure we eventually show the pod row, even if some image is stuck.
      setTimeout(function() {
        $('pod-row').classList.remove('images-loading');
      }, POD_ROW_IMAGES_LOAD_TIMEOUT_MS);

      var isCrosAccountPicker = $('login-header-bar').signinUIState ==
          SIGNIN_UI_STATE.ACCOUNT_PICKER;
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;

      // Chrome OS: immediately recalculate pods layout only when current UI
      //            is account picker. Otherwise postpone it.
      // Desktop: recalculate pods layout right away.
      if (isDesktopUserManager || isCrosAccountPicker) {
        this.placePods_();

        // Without timeout changes in pods positions will be animated even
        // though it happened when 'flying-pods' class was disabled.
        setTimeout(function() {
          Oobe.getInstance().toggleClass('flying-pods', true);
        }, 0);

        // On desktop, don't pre-select a pod if it's the only one.
        if (isDesktopUserManager && this.pods.length == 1)
          this.focusPod();
        else
          this.focusPod(this.preselectedPod);
      } else {
        this.podPlacementPostponed_ = true;

        // Update [Cancel] button state.
        if ($('login-header-bar').signinUIState ==
                SIGNIN_UI_STATE.GAIA_SIGNIN &&
            emptyPodRow &&
            this.pods.length > 0) {
          login.GaiaSigninScreen.updateCancelButtonState();
        }
      }
    },

    /**
     * Adds given apps to the pod row.
     * @param {array} apps Array of apps.
     */
    setApps: function(apps) {
      this.apps_ = apps;
      this.rebuildPods();
      chrome.send('kioskAppsLoaded');

      // Check whether there's a pending kiosk app error.
      window.setTimeout(function() {
        chrome.send('checkKioskAppLaunchError');
      }, 500);
    },

    /**
     * Sets whether should show app pods.
     * @param {boolean} shouldShowApps Whether app pods should be shown.
     */
    setShouldShowApps: function(shouldShowApps) {
      if (this.shouldShowApps_ == shouldShowApps)
        return;

      this.shouldShowApps_ = shouldShowApps;
      this.rebuildPods();
    },

    /**
     * Shows a custom icon on a user pod besides the input field.
     * @param {string} username Username of pod to add button
     * @param {{scale1x: string, scale2x: string}} icon Dictionary of URLs of
     *     the custom icon's representations for 1x and 2x scale factors.
     */
    showUserPodCustomIcon: function(username, icon) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to show user pod button for ' + username +
                      ': user pod not found.');
        return;
      }

      pod.customIconElement.hidden = false;
      pod.customIconElement.style.backgroundImage =
          '-webkit-image-set(' +
              'url(' + icon.scale1x + ') 1x,' +
              'url(' + icon.scale2x + ') 2x)';
    },

    /**
     * Hides the custom icon in the user pod added by showUserPodCustomIcon().
     * @param {string} username Username of pod to remove button
     */
    hideUserPodCustomIcon: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to hide user pod button for ' + username +
                      ': user pod not found.');
        return;
      }

      pod.customIconElement.hidden = true;
    },

    /**
     * Sets the authentication type used to authenticate the user.
     * @param {string} username Username of selected user
     * @param {number} authType Authentication type, must be one of the
     *                          values listed in AUTH_TYPE enum.
     * @param {string} value The initial value to use for authentication.
     */
    setAuthType: function(username, authType, value) {
      var pod = this.getPodWithUsername_(username);
      if (pod == null) {
        console.error('Unable to set auth type for ' + username +
                      ': user pod not found.');
        return;
      }
      pod.setAuthType(authType, value);
    },

    /**
     * Shows a tooltip bubble explaining Easy Unlock for the focused pod.
     */
    showEasyUnlockBubble: function() {
      if (!this.focusedPod_) {
        console.error('No focused pod to show Easy Unlock bubble.');
        return;
      }

      var bubbleContent = document.createElement('div');
      bubbleContent.classList.add('easy-unlock-button-content');
      bubbleContent.textContent = loadTimeData.getString('easyUnlockTooltip');

      var attachElement = this.focusedPod_.customIconElement;
      /** @const */ var BUBBLE_OFFSET = 20;
      /** @const */ var BUBBLE_PADDING = 8;
      $('bubble').showContentForElement(attachElement,
                                        cr.ui.Bubble.Attachment.RIGHT,
                                        bubbleContent,
                                        BUBBLE_OFFSET,
                                        BUBBLE_PADDING);
    },

    /**
     * Called when window was resized.
     */
    onWindowResize: function() {
      var layout = this.calculateLayout_();
      if (layout.columns != this.columns || layout.rows != this.rows)
        this.placePods_();

      if (Oobe.getInstance().virtualKeyboardShown)
        this.scrollFocusedPodIntoView();
    },

    /**
     * Returns width of podrow having |columns| number of columns.
     * @private
     */
    columnsToWidth_: function(columns) {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var margin = isDesktopUserManager ? DESKTOP_MARGIN_BY_COLUMNS[columns] :
                                          MARGIN_BY_COLUMNS[columns];
      var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                              POD_ROW_PADDING;
      return 2 * rowPadding + columns * this.userPodWidth_ +
          (columns - 1) * margin;
    },

    /**
     * Returns height of podrow having |rows| number of rows.
     * @private
     */
    rowsToHeight_: function(rows) {
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                              POD_ROW_PADDING;
      return 2 * rowPadding + rows * this.userPodHeight_;
    },

    /**
     * Calculates number of columns and rows that podrow should have in order to
     * hold as much its pods as possible for current screen size. Also it tries
     * to choose layout that looks good.
     * @return {{columns: number, rows: number}}
     */
    calculateLayout_: function() {
      var preferredColumns = this.pods.length < COLUMNS.length ?
          COLUMNS[this.pods.length] : COLUMNS[COLUMNS.length - 1];
      var maxWidth = Oobe.getInstance().clientAreaSize.width;
      var columns = preferredColumns;
      while (maxWidth < this.columnsToWidth_(columns) && columns > 1)
        --columns;
      var rows = Math.floor((this.pods.length - 1) / columns) + 1;
      if (getComputedStyle(
          $('signin-banner'), null).getPropertyValue('display') != 'none') {
        rows = Math.min(rows, MAX_NUMBER_OF_ROWS_UNDER_SIGNIN_BANNER);
      }
      var maxHeigth = Oobe.getInstance().clientAreaSize.height;
      while (maxHeigth < this.rowsToHeight_(rows) && rows > 1)
        --rows;
      // One more iteration if it's not enough cells to place all pods.
      while (maxWidth >= this.columnsToWidth_(columns + 1) &&
             columns * rows < this.pods.length &&
             columns < MAX_NUMBER_OF_COLUMNS) {
         ++columns;
      }
      return {columns: columns, rows: rows};
    },

    /**
     * Places pods onto their positions onto pod grid.
     * @private
     */
    placePods_: function() {
      var layout = this.calculateLayout_();
      var columns = this.columns = layout.columns;
      var rows = this.rows = layout.rows;
      var maxPodsNumber = columns * rows;
      var isDesktopUserManager = Oobe.getInstance().displayType ==
          DISPLAY_TYPE.DESKTOP_USER_MANAGER;
      var margin = isDesktopUserManager ? DESKTOP_MARGIN_BY_COLUMNS[columns] :
                                          MARGIN_BY_COLUMNS[columns];
      this.parentNode.setPreferredSize(
          this.columnsToWidth_(columns), this.rowsToHeight_(rows));
      var height = this.userPodHeight_;
      var width = this.userPodWidth_;
      this.pods.forEach(function(pod, index) {
        if (pod.offsetHeight != height) {
          console.error('Pod offsetHeight (' + pod.offsetHeight +
              ') and POD_HEIGHT (' + height + ') are not equal.');
        }
        if (pod.offsetWidth != width) {
          console.error('Pod offsetWidth (' + pod.offsetWidth +
              ') and POD_WIDTH (' + width + ') are not equal.');
        }
        if (index >= maxPodsNumber) {
           pod.hidden = true;
           return;
        }
        pod.hidden = false;
        var column = index % columns;
        var row = Math.floor(index / columns);
        var rowPadding = isDesktopUserManager ? DESKTOP_ROW_PADDING :
                                                POD_ROW_PADDING;
        pod.left = rowPadding + column * (width + margin);

        // On desktop, we want the rows to always be equally spaced.
        pod.top = isDesktopUserManager ? row * (height + rowPadding) :
                                         row * height + rowPadding;
      });
      Oobe.getInstance().updateScreenSize(this.parentNode);
    },

    /**
     * Number of columns.
     * @type {?number}
     */
    set columns(columns) {
      // Cannot use 'columns' here.
      this.setAttribute('ncolumns', columns);
    },
    get columns() {
      return parseInt(this.getAttribute('ncolumns'));
    },

    /**
     * Number of rows.
     * @type {?number}
     */
    set rows(rows) {
      // Cannot use 'rows' here.
      this.setAttribute('nrows', rows);
    },
    get rows() {
      return parseInt(this.getAttribute('nrows'));
    },

    /**
     * Whether the pod is currently focused.
     * @param {UserPod} pod Pod to check for focus.
     * @return {boolean} Pod focus status.
     */
    isFocused: function(pod) {
      return this.focusedPod_ == pod;
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod=} podToFocus User pod to focus (undefined clears focus).
     * @param {boolean=} opt_force If true, forces focus update even when
     *     podToFocus is already focused.
     */
    focusPod: function(podToFocus, opt_force) {
      if (this.isFocused(podToFocus) && !opt_force) {
        // Calling focusPod w/o podToFocus means reset.
        if (!podToFocus)
          Oobe.clearErrors();
        this.keyboardActivated_ = false;
        return;
      }

      // Make sure that we don't focus pods that are not allowed to be focused.
      // TODO(nkostylev): Fix various keyboard focus related issues caused
      // by this approach. http://crbug.com/339042
      if (podToFocus && podToFocus.classList.contains('not-focusable')) {
        this.keyboardActivated_ = false;
        return;
      }

      // Make sure there's only one focusPod operation happening at a time.
      if (this.insideFocusPod_) {
        this.keyboardActivated_ = false;
        return;
      }
      this.insideFocusPod_ = true;

      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (!this.alwaysFocusSinglePod) {
          pod.isActionBoxMenuActive = false;
        }
        if (pod != podToFocus) {
          pod.isActionBoxMenuHovered = false;
          pod.classList.remove('focused');
          // On Desktop, the faded style is not set correctly, so we should
          // manually fade out non-focused pods.
          if (pod.user.isDesktopUser)
            pod.classList.add('faded');
          else
            pod.classList.remove('faded');
          pod.reset(false);
        }
      }

      // Clear any error messages for previous pod.
      if (!this.isFocused(podToFocus))
        Oobe.clearErrors();

      var hadFocus = !!this.focusedPod_;
      this.focusedPod_ = podToFocus;
      if (podToFocus) {
        podToFocus.classList.remove('faded');
        podToFocus.classList.add('focused');
        podToFocus.reset(true);  // Reset and give focus.
        // focusPod() automatically loads wallpaper
        if (!podToFocus.user.isApp)
          chrome.send('focusPod', [podToFocus.user.username]);
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;

        if (Oobe.getInstance().virtualKeyboardShown)
          this.scrollFocusedPodIntoView();
      }
      this.insideFocusPod_ = false;
      this.keyboardActivated_ = false;
    },

    /**
     * Focuses a given user pod by index or clear focus when given null.
     * @param {int=} podToFocus index of User pod to focus.
     * @param {boolean=} opt_force If true, forces focus update even when
     *     podToFocus is already focused.
     */
    focusPodByIndex: function(podToFocus, opt_force) {
      if (podToFocus < this.pods.length)
        this.focusPod(this.pods[podToFocus], opt_force);
    },

    /**
     * Resets wallpaper to the last active user's wallpaper, if any.
     */
    loadLastWallpaper: function() {
      if (this.lastFocusedPod_ && !this.lastFocusedPod_.user.isApp)
        chrome.send('loadWallpaper', [this.lastFocusedPod_.user.username]);
    },

    /**
     * Returns the currently activated pod.
     * @type {UserPod}
     */
    get activatedPod() {
      return this.activatedPod_;
    },

    /**
     * Sets currently activated pod.
     * @param {UserPod} pod Pod to check for focus.
     * @param {Event} e Event object.
     */
    setActivatedPod: function(pod, e) {
      if (pod && pod.activate(e))
        this.activatedPod_ = pod;
    },

    /**
     * The pod of the signed-in user, if any; null otherwise.
     * @type {?UserPod}
     */
    get lockedPod() {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.signedIn)
          return pod;
      }
      return null;
    },

    /**
     * The pod that is preselected on user pod row show.
     * @type {?UserPod}
     */
    get preselectedPod() {
      var lockedPod = this.lockedPod;
      var preselectedPod = PRESELECT_FIRST_POD ?
          lockedPod || this.pods[0] : lockedPod;
      return preselectedPod;
    },

    /**
     * Resets input UI.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.disabled = false;
      if (this.activatedPod_)
        this.activatedPod_.reset(takeFocus);
    },

    /**
     * Restores input focus to current selected pod, if there is any.
     */
    refocusCurrentPod: function() {
      if (this.focusedPod_) {
        this.focusedPod_.focusInput();
      }
    },

    /**
     * Clears focused pod password field.
     */
    clearFocusedPod: function() {
      if (!this.disabled && this.focusedPod_)
        this.focusedPod_.reset(true);
    },

    /**
     * Shows signin UI.
     * @param {string} email Email for signin UI.
     */
    showSigninUI: function(email) {
      // Clear any error messages that might still be around.
      Oobe.clearErrors();
      this.disabled = true;
      this.lastFocusedPod_ = this.getPodWithUsername_(email);
      Oobe.showSigninUI(email);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod)
        pod.updateUserImage();
    },

    /**
     * Handler of click event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      // Clear all menus if the click is outside pod menu and its
      // button area.
      if (!findAncestorByClass(e.target, 'action-box-menu') &&
          !findAncestorByClass(e.target, 'action-box-area')) {
        for (var i = 0, pod; pod = this.pods[i]; ++i)
          pod.isActionBoxMenuActive = false;
      }

      // Clears focus if not clicked on a pod and if there's more than one pod.
      var pod = findAncestorByClass(e.target, 'pod');
      if ((!pod || pod.parentNode != this) && !this.alwaysFocusSinglePod) {
        this.focusPod();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Return focus back to single pod.
      if (this.alwaysFocusSinglePod) {
        this.focusPod(this.focusedPod_, true /* force */);
        if (!pod)
          this.focusedPod_.isActionBoxMenuHovered = false;
      }
    },

    /**
     * Handler of mouse move event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleMouseMove_: function(e) {
      if (this.disabled)
        return;
      if (e.webkitMovementX == 0 && e.webkitMovementY == 0)
        return;

      // Defocus (thus hide) action box, if it is focused on a user pod
      // and the pointer is not hovering over it.
      var pod = findAncestorByClass(e.target, 'pod');
      if (document.activeElement &&
          document.activeElement.parentNode != pod &&
          document.activeElement.classList.contains('action-box-area')) {
        document.activeElement.parentNode.focus();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Hide action boxes on other user pods.
      for (var i = 0, p; p = this.pods[i]; ++i)
        if (p != pod && !p.isActionBoxMenuActive)
          p.isActionBoxMenuHovered = false;
    },

    /**
     * Handles focus event.
     * @param {Event} e Focus Event object.
     * @private
     */
    handleFocus_: function(e) {
      if (this.disabled)
        return;
      if (e.target.parentNode == this) {
        // Focus on a pod
        if (e.target.classList.contains('focused'))
          e.target.focusInput();
        else
          this.focusPod(e.target);
        return;
      }

      var pod = findAncestorByClass(e.target, 'pod');
      if (pod && pod.parentNode == this) {
        // Focus on a control of a pod but not on the action area button.
        if (!pod.classList.contains('focused') &&
            !e.target.classList.contains('action-box-button')) {
          this.focusPod(pod);
          e.target.focus();
        }
        return;
      }

      // Clears pod focus when we reach here. It means new focus is neither
      // on a pod nor on a button/input for a pod.
      // Do not "defocus" user pod when it is a single pod.
      // That means that 'focused' class will not be removed and
      // input field/button will always be visible.
      if (!this.alwaysFocusSinglePod)
        this.focusPod();
    },

    /**
     * Handler of keydown event.
     * @param {Event} e KeyDown Event object.
     */
    handleKeyDown: function(e) {
      if (this.disabled)
        return;
      var editing = e.target.tagName == 'INPUT' && e.target.value;
      switch (e.keyIdentifier) {
        case 'Left':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'Right':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          if (this.focusedPod_) {
            var targetTag = e.target.tagName;
            if (e.target == this.focusedPod_.passwordElement ||
                (targetTag != 'INPUT' &&
                 targetTag != 'BUTTON' &&
                 targetTag != 'A')) {
              this.setActivatedPod(this.focusedPod_, e);
              e.stopPropagation();
            }
          }
          break;
        case 'U+001B':  // Esc
          if (!this.alwaysFocusSinglePod)
            this.focusPod();
          break;
      }
    },

    /**
     * Called right after the pod row is shown.
     */
    handleAfterShow: function() {
      // Without timeout changes in pods positions will be animated even though
      // it happened when 'flying-pods' class was disabled.
      setTimeout(function() {
        Oobe.getInstance().toggleClass('flying-pods', true);
      }, 0);
      // Force input focus for user pod on show and once transition ends.
      if (this.focusedPod_) {
        var focusedPod = this.focusedPod_;
        var screen = this.parentNode;
        var self = this;
        focusedPod.addEventListener('webkitTransitionEnd', function f(e) {
          focusedPod.removeEventListener('webkitTransitionEnd', f);
          focusedPod.reset(true);
          // Notify screen that it is ready.
          screen.onShow();
        });
        // Guard timer for 1 second -- it would conver all possible animations.
        ensureTransitionEndEvent(focusedPod, 1000);
      }
    },

    /**
     * Called right before the pod row is shown.
     */
    handleBeforeShow: function() {
      Oobe.getInstance().toggleClass('flying-pods', false);
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = UserPodTabOrder.HEADER_BAR;

      if (this.podPlacementPostponed_) {
        this.podPlacementPostponed_ = false;
        this.placePods_();
        this.focusPod(this.preselectedPod);
      }
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = 0;
    },

    /**
     * Called when a pod's user image finishes loading.
     */
    handlePodImageLoad: function(pod) {
      var index = this.podsWithPendingImages_.indexOf(pod);
      if (index == -1) {
        return;
      }

      this.podsWithPendingImages_.splice(index, 1);
      if (this.podsWithPendingImages_.length == 0) {
        this.classList.remove('images-loading');
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
