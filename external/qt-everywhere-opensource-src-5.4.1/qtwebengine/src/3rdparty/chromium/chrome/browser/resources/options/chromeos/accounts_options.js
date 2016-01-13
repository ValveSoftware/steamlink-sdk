// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // AccountsOptions class:

  /**
   * Encapsulated handling of ChromeOS accounts options page.
   * @constructor
   */
  function AccountsOptions(model) {
    OptionsPage.call(this, 'accounts',
                     loadTimeData.getString('accountsPageTabTitle'),
                     'accountsPage');
    // Whether to show the whitelist.
    this.showWhitelist_ = false;
  }

  cr.addSingletonGetter(AccountsOptions);

  AccountsOptions.prototype = {
    // Inherit AccountsOptions from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes AccountsOptions page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up accounts page.
      var userList = $('userList');
      userList.addEventListener('remove', this.handleRemoveUser_);

      var userNameEdit = $('userNameEdit');
      options.accounts.UserNameEdit.decorate(userNameEdit);
      userNameEdit.addEventListener('add', this.handleAddUser_);

      // If the current user is not the owner, do not show the user list.
      // If the current user is not the owner, or the device is enterprise
      // managed, show a warning that settings cannot be modified.
      this.showWhitelist_ = UIAccountTweaks.currentUserIsOwner();
      if (this.showWhitelist_) {
        options.accounts.UserList.decorate(userList);
      } else {
        $('ownerOnlyWarning').hidden = false;
        this.managed = AccountsOptions.whitelistIsManaged();
      }

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      $('useWhitelistCheck').addEventListener('change',
          this.handleUseWhitelistCheckChange_.bind(this));

      Preferences.getInstance().addEventListener(
          $('useWhitelistCheck').pref,
          this.handleUseWhitelistPrefChange_.bind(this));

      $('accounts-options-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /**
     * Update user list control state.
     * @private
     */
    updateControls_: function() {
      $('userList').disabled =
      $('userNameEdit').disabled = !this.showWhitelist_ ||
                                   AccountsOptions.whitelistIsManaged() ||
                                   !$('useWhitelistCheck').checked;
    },

    /**
     * Handler for OptionsPage's visible property change event.
     * @private
     * @param {Event} e Property change event.
     */
    handleVisibleChange_: function(e) {
      if (this.visible) {
        this.updateControls_();
        if (this.showWhitelist_)
          $('userList').redraw();
      }
    },

    /**
     * Handler for allow guest check change.
     * @private
     */
    handleUseWhitelistCheckChange_: function(e) {
      // Whitelist existing users when guest login is being disabled.
      if ($('useWhitelistCheck').checked) {
        chrome.send('whitelistExistingUsers');
      }

      this.updateControls_();
    },

    /**
     * handler for allow guest pref change.
     * @private
     */
    handleUseWhitelistPrefChange_: function(e) {
      this.updateControls_();
    },

    /**
     * Handler for "add" event fired from userNameEdit.
     * @private
     * @param {Event} e Add event fired from userNameEdit.
     */
    handleAddUser_: function(e) {
      chrome.send('whitelistUser', [e.user.email, e.user.name]);
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_WhitelistedUser_Add']);
    },

    /**
     * Handler for "remove" event fired from userList.
     * @private
     * @param {Event} e Remove event fired from userList.
     */
    handleRemoveUser_: function(e) {
      chrome.send('unwhitelistUser', [e.user.username]);
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_WhitelistedUser_Remove']);
    }
  };


  /**
   * Returns whether the whitelist is managed by policy or not.
   */
  AccountsOptions.whitelistIsManaged = function() {
    return loadTimeData.getBoolean('whitelist_is_managed');
  };

  /**
   * Update account picture.
   * @param {string} username User for which to update the image.
   */
  AccountsOptions.updateAccountPicture = function(username) {
    if (this.showWhitelist_)
      $('userList').updateAccountPicture(username);
  };

  // Export
  return {
    AccountsOptions: AccountsOptions
  };

});
