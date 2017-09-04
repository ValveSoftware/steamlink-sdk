// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.PasswordUiEntry}}} */
var PasswordUiEntryEvent;

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionPair}}} */
var ExceptionPairEntryEvent;

(function() {
'use strict';

Polymer({
  is: 'passwords-section',

  behaviors: [settings.GlobalScrollTargetBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An array of passwords to display.
     * @type {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Whether passwords can be shown or not.
     * @type {boolean}
     */
    showPasswords: {
      type: Boolean,
    },

    /**
     * An array of sites to display.
     * @type {!Array<!chrome.passwordsPrivate.ExceptionPair>}
     */
    passwordExceptions: {
      type: Array,
      value: function() { return []; },
    },


    /**
     * The model for any password related action menus or dialogs.
     * @private {?chrome.passwordsPrivate.PasswordUiEntry}
     */
    activePassword: Object,

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },
  },

  /**
   * Sets the password in the current password dialog if the loginPair matches.
   * @param {!chrome.passwordsPrivate.LoginPair} loginPair
   * @param {string} password
   */
  setPassword: function(loginPair, password) {
    if (this.activePassword &&
        this.activePassword.loginPair.originUrl == loginPair.originUrl &&
        this.activePassword.loginPair.username == loginPair.username) {
      this.$$('password-edit-dialog').password = password;
    }
  },

  /**
   * Shows the edit password dialog.
   * @private
   */
  onMenuEditPasswordTap_: function() {
    /** @type {CrActionMenuElement} */(this.$.menu).close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_: function() {
    this.showPasswordEditDialog_ = false;
  },

  /**
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} savedPasswords
   * @param {string} filter
   * @return {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_: function(savedPasswords, filter) {
    if (!filter)
      return savedPasswords;

    return savedPasswords.filter(function(password) {
      return password.loginPair.originUrl.includes(filter) ||
             password.loginPair.username.includes(filter);
    });
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionPair): boolean}
   * @private
   */
  passwordExceptionFilter_: function(filter) {
    return function(exception) {
      return exception.exceptionUrl.includes(filter);
    };
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_: function() {
    this.fire('remove-saved-password', this.activePassword.loginPair);
    /** @type {CrActionMenuElement} */(this.$.menu).close();
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!ExceptionPairEntryEvent} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_: function(e) {
    this.fire('remove-password-exception', e.model.item.exceptionUrl);
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_: function(length) { return ' '.repeat(length); },

  /**
   * Opens the password action menu.
   * @private
   */
  onPasswordMenuTap_: function(e) {
    var menu = /** @type {!CrActionMenuElement} */(this.$.menu);
    var target = /** @type {!Element} */(Polymer.dom(e).localTarget);
    var passwordUiEntryEvent = /** @type {!PasswordUiEntryEvent} */(e);

    this.activePassword =
        /** @type {!chrome.passwordsPrivate.PasswordUiEntry} */ (
            passwordUiEntryEvent.model.item);
    menu.showAt(target);
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_: function(list) {
    return !!(list && list.length);
  },
});
})();
