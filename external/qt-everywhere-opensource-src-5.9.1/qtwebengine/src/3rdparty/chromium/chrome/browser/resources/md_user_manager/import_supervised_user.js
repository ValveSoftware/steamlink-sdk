// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'import-supervised-user' is a dialog that allows user to select
 * a supervised profile from a list of profiles to import on the current device.
 */
(function() {
/**
 * It means no supervised user is selected.
 * @const {number}
 */
var NO_USER_SELECTED = -1;

Polymer({
  is: 'import-supervised-user',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The currently signed in user and the custodian.
     * @private {?SignedInUser}
     */
    signedInUser_: {
      type: Object,
      value: function() { return null; }
    },

    /**
     * The list of supervised users managed by signedInUser_.
     * @private {!Array<!SupervisedUser>}
     */
    supervisedUsers_: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * Index of the selected supervised user.
     * @private {number}
     */
    supervisedUserIndex_: {
      type: Number,
      value: NO_USER_SELECTED
    }
  },

  /** override */
  ready: function() {
    this.$.dialog.lastFocusableNode = this.$.cancel;
  },

  /**
   * Displays the dialog.
   * @param {(!SignedInUser|undefined)} signedInUser
   * @param {!Array<!SupervisedUser>} supervisedUsers
   */
  show: function(signedInUser, supervisedUsers) {
    this.supervisedUsers_ = supervisedUsers;
    this.supervisedUsers_.sort(function(a, b) {
      if (a.onCurrentDevice != b.onCurrentDevice)
        return a.onCurrentDevice ? 1 : -1;
      return a.name.localeCompare(b.name);
    });

    this.supervisedUserIndex_ = NO_USER_SELECTED;

    this.signedInUser_ = signedInUser || null;
    if (this.signedInUser_)
      this.$.dialog.open();
  },

  /**
   * param {number} supervisedUserIndex Index of the selected supervised user.
   * @private
   * @return {boolean} Whether the 'Import' button should be disabled.
   */
  isImportDisabled_: function(supervisedUserIndex) {
    var disabled = supervisedUserIndex == NO_USER_SELECTED;
    if (!disabled) {
      this.$.dialog.lastFocusableNode = this.$.import;
    }
    return disabled;
  },

  /**
   * Called when the user clicks the 'Import' button. it proceeds with importing
   * the supervised user.
   * @private
   */
  onImportTap_: function() {
    var supervisedUser = this.supervisedUsers_[this.supervisedUserIndex_];
    if (this.signedInUser_ && supervisedUser) {
      this.$.dialog.close();
      // Event is caught by create-profile.
      this.fire('import', {supervisedUser: supervisedUser,
                           signedInUser: this.signedInUser_});
    }
  }
});
})();
