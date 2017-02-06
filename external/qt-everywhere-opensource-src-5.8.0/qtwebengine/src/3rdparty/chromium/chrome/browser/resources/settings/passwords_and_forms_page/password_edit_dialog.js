// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

(function() {
'use strict';

Polymer({
  is: 'password-edit-dialog',

  properties: {
    /**
     * The password that is being displayed.
     * @type {!chrome.passwordsPrivate.PasswordUiEntry}
     */
    item: Object,

    /** Holds the plaintext password when requested. */
    password: String,
  },

  /** Opens the dialog. */
  open: function() {
    this.password = '';
    this.$.dialog.open();
  },

  /** Closes the dialog. */
  close: function() {
    this.$.dialog.close();
  },

  /**
   * Gets the password input's type. Should be 'text' when password is visible
   * and 'password' when it's not.
   * @param {string} password
   * @private
   */
  getPasswordInputType_: function(password) {
    return password ? 'text' : 'password';
  },

  /**
   * Gets the text of the password. Will use the value of |password| unless it
   * cannot be shown, in which case it will be spaces.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} item
   * @param {string} password
   * @private
   */
  getPassword_: function(item, password) {
    if (password)
      return password;
    return item ? ' '.repeat(item.numCharactersInPassword) : '';
  },

  /**
   * Handler for tapping the show/hide button. Will fire an event to request the
   * password for this login pair.
   * @private
   */
  onShowPasswordButtonTap_: function() {
    if (this.password)
      this.password = '';
    else
      this.fire('show-password', this.item.loginPair);  // Request the password.
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancelButtonTap_: function() {
    this.close();
  },

  /**
   * Handler for tapping the save button.
   * @private
   */
  onSaveButtonTap_: function() {
    // TODO(hcarmona): what to save?
    this.close();
  },
});
})();
