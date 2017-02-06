// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * SupervisedUserImportOverlay class.
   * Encapsulated handling of the 'Import existing supervised user' overlay
   * page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function SupervisedUserImportOverlay() {
    var title = loadTimeData.getString('supervisedUserImportTitle');
    Page.call(this, 'supervisedUserImport', title, 'supervised-user-import');
  };

  cr.addSingletonGetter(SupervisedUserImportOverlay);

  SupervisedUserImportOverlay.prototype = {
    // Inherit from Page.
    __proto__: Page.prototype,

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isSupervised;
    },

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var supervisedUserList = $('supervised-user-list');
      options.supervisedUserOptions.SupervisedUserList.decorate(
          supervisedUserList);

      var avatarGrid = $('select-avatar-grid');
      options.ProfilesIconGrid.decorate(avatarGrid);
      var avatarIcons = loadTimeData.getValue('avatarIcons');
      avatarGrid.dataModel = new ArrayDataModel(
          /** @type {!Array} */(avatarIcons));

      supervisedUserList.addEventListener('change', function(event) {
        var supervisedUser = supervisedUserList.selectedItem;
        $('supervised-user-import-ok').disabled =
            !supervisedUser || supervisedUser.onCurrentDevice;
      });

      var self = this;
      $('supervised-user-import-cancel').onclick = function(event) {
        if (self.inProgress_) {
          self.updateImportInProgress_(false);

          // 'cancelCreateProfile' is handled by CreateProfileHandler.
          chrome.send('cancelCreateProfile');
        }
        PageManager.closeOverlay();
      };

      $('supervised-user-import-ok').onclick =
          this.showAvatarGridOrSubmit_.bind(this);
      $('supervised-user-select-avatar-ok').onclick =
          this.showAvatarGridOrSubmit_.bind(this);
    },

    /**
     * @override
     */
    didShowPage: function() {
      // When the import link is clicked to open this overlay, it is hidden in
      // order to trigger a cursor update. We can show the import link again
      // now. TODO(akuegel): Remove this temporary fix when crbug/246304 is
      // resolved.
      $('import-existing-supervised-user-link').hidden = false;

      options.SupervisedUserListData.requestExistingSupervisedUsers().then(
          this.receiveExistingSupervisedUsers_.bind(this),
          this.onSigninError_.bind(this));
      options.SupervisedUserListData.addObserver(this);

      this.updateImportInProgress_(false);
      $('supervised-user-import-error-bubble').hidden = true;
      $('supervised-user-import-ok').disabled = true;
      this.showAppropriateElements_(/* isSelectAvatarMode */ false);
    },

    /**
     * @override
     */
    didClosePage: function() {
      options.SupervisedUserListData.removeObserver(this);
    },

    /**
     * Shows either the supervised user import dom elements or the select avatar
     * dom elements.
     * @param {boolean} isSelectAvatarMode True if the overlay should show the
     *     select avatar grid, and false if the overlay should show the
     *     supervised user list.
     * @private
     */
    showAppropriateElements_: function(isSelectAvatarMode) {
      var avatarElements =
          this.pageDiv.querySelectorAll('.supervised-user-select-avatar');
      for (var i = 0; i < avatarElements.length; i++)
        avatarElements[i].hidden = !isSelectAvatarMode;
      var importElements =
          this.pageDiv.querySelectorAll('.supervised-user-import');
      for (var i = 0; i < importElements.length; i++)
        importElements[i].hidden = isSelectAvatarMode;
    },

    /**
     * Called when the user clicks the "OK" button. In case the supervised
     * user being imported has no avatar in sync, it shows the avatar
     * icon grid. In case the avatar grid is visible or the supervised user
     * already has an avatar stored in sync, it proceeds with importing
     * the supervised user.
     * @private
     */
    showAvatarGridOrSubmit_: function() {
      var supervisedUser = $('supervised-user-list').selectedItem;
      if (!supervisedUser)
        return;

      $('supervised-user-import-error-bubble').hidden = true;

      if ($('select-avatar-grid').hidden && supervisedUser.needAvatar) {
        this.showAvatarGridHelper_();
        return;
      }

      var avatarUrl = supervisedUser.needAvatar ?
          $('select-avatar-grid').selectedItem : supervisedUser.iconURL;

      this.updateImportInProgress_(true);

      // 'createProfile' is handled by CreateProfileHandler.
      chrome.send('createProfile', [supervisedUser.name, avatarUrl,
                                    false, true, supervisedUser.id]);
    },

    /**
     * Hides the 'supervised user list' and shows the avatar grid instead.
     * It also updates the overlay text and title to instruct the user
     * to choose an avatar for the supervised user.
     * @private
     */
    showAvatarGridHelper_: function() {
      this.showAppropriateElements_(/* isSelectAvatarMode */ true);
      $('select-avatar-grid').redraw();
      $('select-avatar-grid').selectedItem =
          loadTimeData.getValue('avatarIcons')[0];
    },

    /**
     * Updates the UI according to the importing state.
     * @param {boolean} inProgress True to indicate that
     *     importing is in progress and false otherwise.
     * @private
     */
    updateImportInProgress_: function(inProgress) {
      this.inProgress_ = inProgress;
      $('supervised-user-import-ok').disabled = inProgress;
      $('supervised-user-select-avatar-ok').disabled = inProgress;
      $('supervised-user-list').disabled = inProgress;
      $('select-avatar-grid').disabled = inProgress;
      $('supervised-user-import-throbber').hidden = !inProgress;
    },

    /**
     * Sets the data model of the supervised user list to |supervisedUsers|.
     * @param {Array<{id: string, name: string, iconURL: string,
     *     onCurrentDevice: boolean, needAvatar: boolean}>} supervisedUsers
     *     Array of supervised user objects.
     * @private
     */
    receiveExistingSupervisedUsers_: function(supervisedUsers) {
      supervisedUsers.sort(function(a, b) {
        if (a.onCurrentDevice != b.onCurrentDevice)
          return a.onCurrentDevice ? 1 : -1;
        return a.name.localeCompare(b.name);
      });

      $('supervised-user-list').dataModel = new ArrayDataModel(supervisedUsers);
      if (supervisedUsers.length == 0) {
        this.onErrorInternal_(
            loadTimeData.getString('noExistingSupervisedUsers'));
        $('supervised-user-import-ok').disabled = true;
      } else {
        // Hide the error bubble.
        $('supervised-user-import-error-bubble').hidden = true;
      }
    },

    onSigninError_: function() {
      $('supervised-user-list').dataModel = null;
      this.onErrorInternal_(
          loadTimeData.getString('supervisedUserImportSigninError'));
    },

    /**
     * Displays an error message if an error occurs while
     * importing a supervised user.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} error The error message to display.
     * @private
     */
    onError_: function(error) {
      this.onErrorInternal_(error);
      this.updateImportInProgress_(false);
    },

    /**
     * Displays an error message.
     * @param {string} error The error message to display.
     * @private
     */
    onErrorInternal_: function(error) {
      var errorBubble = $('supervised-user-import-error-bubble');
      errorBubble.hidden = false;
      errorBubble.textContent = error;
    },

    /**
     * Closes the overlay if importing the supervised user was successful. Also
     * reset the cached list of supervised users in order to get an updated list
     * when the overlay is reopened.
     * @private
     */
    onSuccess_: function() {
      this.updateImportInProgress_(false);
      options.SupervisedUserListData.resetPromise();
      PageManager.closeAllOverlays();
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(SupervisedUserImportOverlay, [
    'onError',
    'onSuccess',
  ]);

  // Export
  return {
    SupervisedUserImportOverlay: SupervisedUserImportOverlay,
  };
});
