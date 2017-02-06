// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-page' is the settings page for managing user accounts on
 * the device.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <settings-users-page prefs="{{prefs}}">
 *      </settings-users-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 */
Polymer({
  is: 'settings-users-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    isOwner_: {
      type: Boolean,
      value: true
    },

    /** @private */
    isWhitelistManaged_: {
      type: Boolean,
      value: false
    },
  },

  keyBindings: {
    'enter': 'addUser_'
  },

  /** @override */
  created: function() {
    chrome.usersPrivate.isCurrentUserOwner(function(isOwner) {
      this.isOwner_ = isOwner;
    }.bind(this));

    chrome.usersPrivate.isWhitelistManaged(function(isWhitelistManaged) {
      this.isWhitelistManaged_ = isWhitelistManaged;
    }.bind(this));
  },

  /** @private */
  openAddUserDialog_: function() {
    this.$.addUserDialog.open();
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   * @return {boolean}
   */
  isOwnerLabelHidden_: function(isOwner, isWhitelistManaged) {
    return isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @private
   * @return {boolean}
   */
  isEditingDisabled_: function(isOwner, isWhitelistManaged) {
    return !isOwner || isWhitelistManaged;
  },

  /**
   * @param {boolean} isOwner
   * @param {boolean} isWhitelistManaged
   * @param {boolean} allowGuest
   * @private
   * @return {boolean}
   */
  isEditingUsersDisabled_: function(
      isOwner, isWhitelistManaged, allowGuest) {
    return !isOwner || isWhitelistManaged || allowGuest;
  }
});
