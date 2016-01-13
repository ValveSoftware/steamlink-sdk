// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * ManagedUserImportOverlay class.
   * Encapsulated handling of the 'Import existing managed user' overlay page.
   * @constructor
   * @class
   */
  function ManagedUserImportOverlay() {
    var title = loadTimeData.getString('managedUserImportTitle');
    OptionsPage.call(this, 'managedUserImport',
                     title, 'managed-user-import');
  };

  cr.addSingletonGetter(ManagedUserImportOverlay);

  ManagedUserImportOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isManaged;
    },

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var managedUserList = $('managed-user-list');
      options.managedUserOptions.ManagedUserList.decorate(managedUserList);

      var avatarGrid = $('select-avatar-grid');
      options.ProfilesIconGrid.decorate(avatarGrid);
      var avatarIcons = loadTimeData.getValue('avatarIcons');
      avatarGrid.dataModel = new ArrayDataModel(avatarIcons);

      managedUserList.addEventListener('change', function(event) {
        var managedUser = managedUserList.selectedItem;
        if (!managedUser)
          return;

        $('managed-user-import-ok').disabled =
            managedUserList.selectedItem.onCurrentDevice;
      });

      var self = this;
      $('managed-user-import-cancel').onclick = function(event) {
        if (self.inProgress_) {
          self.updateImportInProgress_(false);

          // 'cancelCreateProfile' is handled by CreateProfileHandler.
          chrome.send('cancelCreateProfile');
        }
        OptionsPage.closeOverlay();
      };

      $('managed-user-import-ok').onclick =
          this.showAvatarGridOrSubmit_.bind(this);
      $('managed-user-select-avatar-ok').onclick =
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
      $('import-existing-managed-user-link').hidden = false;

      options.ManagedUserListData.requestExistingManagedUsers().then(
          this.receiveExistingManagedUsers_, this.onSigninError_.bind(this));
      options.ManagedUserListData.addObserver(this);

      this.updateImportInProgress_(false);
      $('managed-user-import-error-bubble').hidden = true;
      $('managed-user-import-ok').disabled = true;
      this.showAppropriateElements_(/* isSelectAvatarMode */ false);
    },

    /**
     * @override
     */
    didClosePage: function() {
      options.ManagedUserListData.removeObserver(this);
    },

    /**
     * Shows either the managed user import dom elements or the select avatar
     * dom elements.
     * @param {boolean} isSelectAvatarMode True if the overlay should show the
     *     select avatar grid, and false if the overlay should show the managed
     *     user list.
     * @private
     */
    showAppropriateElements_: function(isSelectAvatarMode) {
      var avatarElements =
          this.pageDiv.querySelectorAll('.managed-user-select-avatar');
      for (var i = 0; i < avatarElements.length; i++)
        avatarElements[i].hidden = !isSelectAvatarMode;
      var importElements =
          this.pageDiv.querySelectorAll('.managed-user-import');
      for (var i = 0; i < importElements.length; i++)
        importElements[i].hidden = isSelectAvatarMode;
    },

    /**
     * Called when the user clicks the "OK" button. In case the managed
     * user being imported has no avatar in sync, it shows the avatar
     * icon grid. In case the avatar grid is visible or the managed user
     * already has an avatar stored in sync, it proceeds with importing
     * the managed user.
     * @private
     */
    showAvatarGridOrSubmit_: function() {
      var managedUser = $('managed-user-list').selectedItem;
      if (!managedUser)
        return;

      $('managed-user-import-error-bubble').hidden = true;

      if ($('select-avatar-grid').hidden && managedUser.needAvatar) {
        this.showAvatarGridHelper_();
        return;
      }

      var avatarUrl = managedUser.needAvatar ?
          $('select-avatar-grid').selectedItem : managedUser.iconURL;

      this.updateImportInProgress_(true);

      // 'createProfile' is handled by CreateProfileHandler.
      chrome.send('createProfile', [managedUser.name, avatarUrl,
                                    false, true, managedUser.id]);
    },

    /**
     * Hides the 'managed user list' and shows the avatar grid instead.
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
      $('managed-user-import-ok').disabled = inProgress;
      $('managed-user-select-avatar-ok').disabled = inProgress;
      $('managed-user-list').disabled = inProgress;
      $('select-avatar-grid').disabled = inProgress;
      $('managed-user-import-throbber').hidden = !inProgress;
    },

    /**
     * Sets the data model of the managed user list to |managedUsers|.
     * @param {Array.<Object>} managedUsers An array of managed user objects.
     *     Each object is of the form:
     *       managedUser = {
     *         id: "Managed User ID",
     *         name: "Managed User Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         onCurrentDevice: true or false,
     *         needAvatar: true or false
     *       }
     * @private
     */
    receiveExistingManagedUsers_: function(managedUsers) {
      managedUsers.sort(function(a, b) {
        if (a.onCurrentDevice != b.onCurrentDevice)
          return a.onCurrentDevice ? 1 : -1;
        return a.name.localeCompare(b.name);
      });

      $('managed-user-list').dataModel = new ArrayDataModel(managedUsers);
      if (managedUsers.length == 0) {
        this.onError_(loadTimeData.getString('noExistingManagedUsers'));
        $('managed-user-import-ok').disabled = true;
      } else {
        // Hide the error bubble.
        $('managed-user-import-error-bubble').hidden = true;
      }
    },

    onSigninError_: function() {
      $('managed-user-list').dataModel = null;
      this.onError_(loadTimeData.getString('managedUserImportSigninError'));
    },

    /**
     * Displays an error message if an error occurs while
     * importing a managed user.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} error The error message to display.
     * @private
     */
    onError_: function(error) {
      var errorBubble = $('managed-user-import-error-bubble');
      errorBubble.hidden = false;
      errorBubble.textContent = error;
      this.updateImportInProgress_(false);
    },

    /**
     * Closes the overlay if importing the managed user was successful. Also
     * reset the cached list of managed users in order to get an updated list
     * when the overlay is reopened.
     * @private
     */
    onSuccess_: function() {
      this.updateImportInProgress_(false);
      options.ManagedUserListData.resetPromise();
      OptionsPage.closeAllOverlays();
    },
  };

  // Forward public APIs to private implementations.
  [
    'onSuccess',
  ].forEach(function(name) {
    ManagedUserImportOverlay[name] = function() {
      var instance = ManagedUserImportOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserImportOverlay: ManagedUserImportOverlay,
  };
});
