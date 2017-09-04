// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * ManageProfileOverlay class
   * Encapsulated handling of the 'Manage profile...' overlay page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ManageProfileOverlay() {
    Page.call(this, 'manageProfile',
              loadTimeData.getString('manageProfileTabTitle'),
              'manage-profile-overlay');
  }

  cr.addSingletonGetter(ManageProfileOverlay);

  ManageProfileOverlay.prototype = {
    // Inherit from Page.
    __proto__: Page.prototype,

    // Info about the currently managed/deleted profile.
    profileInfo_: null,

    // Whether the currently chosen name for a new profile was assigned
    // automatically by choosing an avatar. Set on receiveNewProfileDefaults;
    // cleared on first edit (in onNameChanged_).
    profileNameIsDefault_: false,

    // List of default profile names corresponding to the respective icons.
    defaultProfileNames_: [],

    // An object containing all names of existing profiles.
    existingProfileNames_: {},

    // The currently selected icon in the icon grid.
    iconGridSelectedURL_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var self = this;
      options.ProfilesIconGrid.decorate($('manage-profile-icon-grid'));
      options.ProfilesIconGrid.decorate($('create-profile-icon-grid'));
      self.registerCommonEventHandlers_('create',
                                        self.submitCreateProfile_.bind(self));
      self.registerCommonEventHandlers_('manage',
                                        self.submitManageChanges_.bind(self));

      // Override the create-profile-ok and create-* keydown handlers, to avoid
      // closing the overlay until we finish creating the profile.
      $('create-profile-ok').onclick = function(event) {
        self.submitCreateProfile_();
      };

      $('create-profile-cancel').onclick = function(event) {
        CreateProfileOverlay.cancelCreateProfile();
      };

      $('manage-profile-cancel').onclick =
          $('disconnect-managed-profile-cancel').onclick =
          $('delete-profile-cancel').onclick = function(event) {
        PageManager.closeOverlay();
      };
      $('delete-profile-ok').onclick = function(event) {
        PageManager.closeOverlay();
        chrome.send('deleteProfile', [self.profileInfo_.filePath]);
        options.SupervisedUserListData.resetPromise();
      };
      $('add-shortcut-button').onclick = function(event) {
        chrome.send('addProfileShortcut', [self.profileInfo_.filePath]);
      };
      $('remove-shortcut-button').onclick = function(event) {
        chrome.send('removeProfileShortcut', [self.profileInfo_.filePath]);
      };

      $('disconnect-managed-profile-ok').onclick = function(event) {
        PageManager.closeOverlay();
        chrome.send('deleteProfile',
                    [BrowserOptions.getCurrentProfile().filePath]);
      };

      $('create-profile-supervised-signed-in-learn-more-link').onclick =
          function(event) {
        PageManager.showPageByName('supervisedUserLearnMore');
        return false;
      };

      $('create-profile-supervised-sign-in-link').onclick =
          function(event) {
        SyncSetupOverlay.startSignIn(true /* creatingSupervisedUser */);
      };

      $('create-profile-supervised-sign-in-again-link').onclick =
          function(event) {
        SyncSetupOverlay.showSetupUI();
      };

      $('import-existing-supervised-user-link').onclick = function(event) {
        // Hide the import button to trigger a cursor update. The import button
        // is shown again when the import overlay loads. TODO(akuegel): Remove
        // this temporary fix when crbug/246304 is resolved.
        $('import-existing-supervised-user-link').hidden = true;
        PageManager.showPageByName('supervisedUserImport');
      };
    },

    /** @override */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons', ['manage']);

      // Just ignore the manage profile dialog on Chrome OS, they use /accounts.
      if (!cr.isChromeOS && window.location.pathname == '/manageProfile')
        ManageProfileOverlay.getInstance().prepareForManageDialog_();

      // When editing a profile, initially hide the "add shortcut" and
      // "remove shortcut" buttons and ask the handler which to show. It will
      // call |receiveHasProfileShortcuts|, which will show the appropriate one.
      $('remove-shortcut-button').hidden = true;
      $('add-shortcut-button').hidden = true;

      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        var profileInfo = ManageProfileOverlay.getInstance().profileInfo_;
        chrome.send('requestHasProfileShortcuts', [profileInfo.filePath]);
      }

      var manageNameField = $('manage-profile-name');
      // Legacy supervised users cannot edit their names.
      if (manageNameField.disabled)
        $('manage-profile-ok').focus();
      else
        manageNameField.focus();

      this.profileNameIsDefault_ = false;
    },

    /**
     * Registers event handlers that are common between create and manage modes.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @param {function()} submitFunction The function that should be called
     *     when the user chooses to submit (e.g. by clicking the OK button).
     * @private
     */
    registerCommonEventHandlers_: function(mode, submitFunction) {
      var self = this;
      $(mode + '-profile-icon-grid').addEventListener('change', function(e) {
        self.onIconGridSelectionChanged_(mode);
      });
      $(mode + '-profile-name').oninput = function(event) {
        self.onNameChanged_(mode);
      };
      $(mode + '-profile-ok').onclick = function(event) {
        PageManager.closeOverlay();
        submitFunction();
      };
    },

    /**
     * Set the profile info used in the dialog.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       iconURL: "chrome://path/to/icon/image",
     *       filePath: "/path/to/profile/data/on/disk",
     *       isCurrentProfile: false,
     *       isSupervised: false
     *     };
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    setProfileInfo_: function(profileInfo, mode) {
      this.iconGridSelectedURL_ = profileInfo.iconURL;
      this.profileInfo_ = profileInfo;
      $(mode + '-profile-name').value = profileInfo.name;
      $(mode + '-profile-icon-grid').selectedItem = profileInfo.iconURL;
    },

    /**
     * Sets the name of the profile being edited or created.
     * @param {string} name New profile name.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    setProfileName_: function(name, mode) {
      if (this.profileInfo_)
        this.profileInfo_.name = name;
      $(mode + '-profile-name').value = name;
    },

    /**
     * Set an array of default icon URLs. These will be added to the grid that
     * the user will use to choose their profile icon.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @param {!Array<string>} iconURLs An array of icon URLs.
     * @param {Array<string>} names An array of default names
     *     corresponding to the icons.
     * @private
     */
    receiveDefaultProfileIconsAndNames_: function(mode, iconURLs, names) {
      this.defaultProfileNames_ = names;

      var grid = $(mode + '-profile-icon-grid');

      grid.dataModel = new ArrayDataModel(iconURLs);

      if (this.profileInfo_)
        grid.selectedItem = this.profileInfo_.iconURL;

      // Recalculate the measured item size.
      grid.measured_ = null;
      grid.columns = 0;
      grid.redraw();
    },

    /**
     * Callback to set the initial values when creating a new profile.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       iconURL: "chrome://path/to/icon/image",
     *     };
     * @private
     */
    receiveNewProfileDefaults_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo, 'create');
      this.profileNameIsDefault_ = true;
      $('create-profile-name-label').hidden = false;
      $('create-profile-name').hidden = false;
      // Trying to change the focus if this isn't the topmost overlay can
      // instead cause the FocusManager to override another overlay's focus,
      // e.g. if an overlay above this one is in the process of being reloaded.
      // But the C++ handler calls this method directly on ManageProfileOverlay,
      // so check the pageDiv to also include its subclasses (in particular
      // CreateProfileOverlay, which has higher sub-overlays).
      if (PageManager.getTopmostVisiblePage().pageDiv == this.pageDiv) {
        // This will only have an effect if the 'create-profile-name' element
        //  is visible, i.e. if the overlay is in create mode.
        $('create-profile-name').focus();
      }
      $('create-profile-ok').disabled = false;
    },

    /**
     * Set a dictionary of all profile names. These are used to prevent the
     * user from naming two profiles the same.
     * @param {Object} profileNames A dictionary of profile names.
     * @private
     */
    receiveExistingProfileNames_: function(profileNames) {
      this.existingProfileNames_ = profileNames;
    },

    /**
     * Callback to show the add/remove shortcut buttons when in edit mode,
     * called by the handler as a result of the 'requestHasProfileShortcuts_'
     * message.
     * @param {boolean} hasShortcuts Whether profile has any existing shortcuts.
     * @private
     */
    receiveHasProfileShortcuts_: function(hasShortcuts) {
      $('add-shortcut-button').hidden = hasShortcuts;
      $('remove-shortcut-button').hidden = !hasShortcuts;
    },

    /**
     * Display the error bubble, with |errorHtml| in the bubble.
     * @param {string} errorHtml The html string to display as an error.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @param {boolean} disableOKButton True if the dialog's OK button should be
     *     disabled when the error bubble is shown. It will be (re-)enabled when
     *     the error bubble is hidden.
     * @private
     */
    showErrorBubble_: function(errorHtml, mode, disableOKButton) {
      var nameErrorEl = $(mode + '-profile-error-bubble');
      nameErrorEl.hidden = false;
      nameErrorEl.innerHTML = errorHtml;

      if (disableOKButton)
        $(mode + '-profile-ok').disabled = true;
    },

    /**
     * Hide the error bubble.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    hideErrorBubble_: function(mode) {
      $(mode + '-profile-error-bubble').innerHTML = '';
      $(mode + '-profile-error-bubble').hidden = true;
      $(mode + '-profile-ok').disabled = false;
    },

    /**
     * oninput callback for <input> field.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    onNameChanged_: function(mode) {
      this.profileNameIsDefault_ = false;
      this.updateCreateOrImport_(mode);
    },

    /**
     * Called when the profile name is changed or the 'create supervised'
     * checkbox is toggled. Updates the 'ok' button and the 'import existing
     * supervised user' link.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    updateCreateOrImport_: function(mode) {
      this.updateOkButton_(mode);
      // In 'create' mode, check for existing supervised users with the same
      // name.
      if (mode == 'create')
        this.requestExistingSupervisedUsers_();
    },

    /**
     * Tries to get the list of existing supervised users and updates the UI
     * accordingly.
     * @private
     */
    requestExistingSupervisedUsers_: function() {
      options.SupervisedUserListData.requestExistingSupervisedUsers().then(
          this.receiveExistingSupervisedUsers_.bind(this),
          this.onSigninError_.bind(this));
    },

    /**
     * @param {Object} supervisedUser
     * @param {boolean} nameIsUnique
     */
    getImportHandler_: function(supervisedUser, nameIsUnique) {
      return function() {
        if (supervisedUser.needAvatar || !nameIsUnique) {
          PageManager.showPageByName('supervisedUserImport');
        } else {
          this.hideErrorBubble_('create');
          CreateProfileOverlay.updateCreateInProgress(true);
          chrome.send('createProfile',
              [supervisedUser.name, supervisedUser.iconURL, false, true,
                   supervisedUser.id]);
        }
      }.bind(this);
    },

    /**
     * Callback which receives the list of existing supervised users. Checks if
     * the currently entered name is the name of an already existing supervised
     * user. If yes, the user is prompted to import the existing supervised
     * user, and the create button is disabled.
     * If the received list is empty, hides the "import" link.
     * @param {Array<Object>} supervisedUsers The list of existing supervised
     *     users.
     * @private
     */
    receiveExistingSupervisedUsers_: function(supervisedUsers) {
      // After a supervised user has been created and the dialog has been
      // hidden, this gets called again with a list including
      // the just-created SU. Ignore, to prevent the "already exists" bubble
      // from showing up if the overlay is already hidden.
      if (PageManager.getTopmostVisiblePage().pageDiv != this.pageDiv)
        return;
      $('import-existing-supervised-user-link').hidden =
          supervisedUsers.length === 0;
      if (!$('create-profile-supervised').checked)
        return;

      var newName = $('create-profile-name').value;
      var i;
      for (i = 0; i < supervisedUsers.length; ++i) {
        if (supervisedUsers[i].name != newName)
          continue;
        // Check if another supervised user also exists with that name.
        var nameIsUnique = true;
        // Handling the case when multiple supervised users with the same
        // name exist, but not all of them are on the device.
        // If at least one is not imported, we want to offer that
        // option to the user. This could happen due to a bug that allowed
        // creating SUs with the same name (https://crbug.com/557445).
        var allOnCurrentDevice = supervisedUsers[i].onCurrentDevice;
        var j;
        for (j = i + 1; j < supervisedUsers.length; ++j) {
          if (supervisedUsers[j].name == newName) {
            nameIsUnique = false;
            allOnCurrentDevice = allOnCurrentDevice &&
               supervisedUsers[j].onCurrentDevice;
          }
        }

        var errorHtml = allOnCurrentDevice ?
            loadTimeData.getStringF(
                'managedProfilesExistingLocalSupervisedUser') :
            loadTimeData.getStringF(
                'manageProfilesExistingSupervisedUser',
                HTMLEscape(elide(newName, /* maxLength */ 50)));
        this.showErrorBubble_(errorHtml, 'create', true);

        if ($('supervised-user-import-existing')) {
          $('supervised-user-import-existing').onclick =
              this.getImportHandler_(supervisedUsers[i], nameIsUnique);
        }
        $('create-profile-ok').disabled = true;
        return;
      }
    },

    /**
     * Called in case the request for the list of supervised users fails because
     * of a signin error.
     * @private
     */
    onSigninError_: function() {
      this.updateSignedInStatus(this.signedInEmail_, true);
    },

    /**
     * Called to update the state of the ok button depending if the name is
     * already used or not.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    updateOkButton_: function(mode) {
      var oldName = this.profileInfo_.name;
      var newName = $(mode + '-profile-name').value;
      this.hideErrorBubble_(mode);

      var nameIsValid = $(mode + '-profile-name').validity.valid;
      $(mode + '-profile-ok').disabled = !nameIsValid;
    },

    /**
     * Called when the user clicks "OK" or hits enter. Saves the newly changed
     * profile info.
     * @private
     */
    submitManageChanges_: function() {
      var name = $('manage-profile-name').value;
      var iconURL = $('manage-profile-icon-grid').selectedItem;

      chrome.send('setProfileIconAndName',
                  [this.profileInfo_.filePath, iconURL, name]);
      if (name != this.profileInfo_.name)
        options.SupervisedUserListData.resetPromise();
    },

    /**
     * Abstract method. Should be overriden in subclasses.
     * @param {string} email
     * @param {boolean} hasError
     * @protected
     */
    updateSignedInStatus: function(email, hasError) {
      // TODO: Fix triggering the assert, crbug.com/423267
      // assertNotReached();
    },

    /**
     * Called when the user clicks "OK" or hits enter. Creates the profile
     * using the information in the dialog.
     * @private
     */
    submitCreateProfile_: function() {
      // This is visual polish: the UI to access this should be disabled for
      // supervised users, and the back end will prevent user creation anyway.
      if (this.profileInfo_ && this.profileInfo_.isSupervised)
        return;

      this.hideErrorBubble_('create');
      CreateProfileOverlay.updateCreateInProgress(true);

      // Get the user's chosen name and icon, or default if they do not
      // wish to customize their profile.
      var name = $('create-profile-name').value;
      var iconUrl = $('create-profile-icon-grid').selectedItem;
      var createShortcut = $('create-shortcut').checked;
      var isSupervised = $('create-profile-supervised').checked;
      var existingSupervisedUserId = '';

      // 'createProfile' is handled by the CreateProfileHandler.
      chrome.send('createProfile',
                  [name, iconUrl, createShortcut,
                   isSupervised, existingSupervisedUserId]);
    },

    /**
     * Called when the selected icon in the icon grid changes.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    onIconGridSelectionChanged_: function(mode) {
      var iconURL = $(mode + '-profile-icon-grid').selectedItem;
      if (!iconURL || iconURL == this.iconGridSelectedURL_)
        return;
      this.iconGridSelectedURL_ = iconURL;
      if (this.profileNameIsDefault_) {
        var index = $(mode + '-profile-icon-grid').selectionModel.selectedIndex;
        var name = this.defaultProfileNames_[index];
        if (name) {
          this.setProfileName_(name, mode);
          this.updateCreateOrImport_(mode);
        }
      }
      if (this.profileInfo_ && this.profileInfo_.filePath) {
        chrome.send('profileIconSelectionChanged',
                    [this.profileInfo_.filePath, iconURL]);
      }
    },

    /**
     * Updates the contents of the "Manage Profile" section of the dialog,
     * and shows that section.
     * @private
     */
    prepareForManageDialog_: function() {
      chrome.send('refreshGaiaPicture');
      var profileInfo = BrowserOptions.getCurrentProfile();
      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = false;
      $('manage-profile-overlay-delete').hidden = true;
      $('manage-profile-overlay-disconnect-managed').hidden = true;
      $('manage-profile-name').disabled =
          profileInfo.isSupervised && !profileInfo.isChild;
      this.hideErrorBubble_('manage');
    },

    /**
     * Display the "Manage Profile" dialog.
     * @param {boolean=} opt_updateHistory If we should update the history after
     *     showing the dialog (defaults to true).
     * @private
     */
    showManageDialog_: function(opt_updateHistory) {
      var updateHistory = opt_updateHistory !== false;
      this.prepareForManageDialog_();
      PageManager.showPageByName('manageProfile', updateHistory);
    },

    /**
     * Display the "Delete Profile" dialog.
     * @param {Object} profileInfo The profile object of the profile to delete.
     * @private
     */
    showDeleteDialog_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = false;
      $('manage-profile-overlay-disconnect-managed').hidden = true;
      $('delete-profile-icon').style.content =
          cr.icon.getImage(profileInfo.iconURL);
      $('delete-profile-text').textContent =
          loadTimeData.getStringF('deleteProfileMessage',
                                  elide(profileInfo.name, /* maxLength */ 50));
      $('delete-supervised-profile-addendum').hidden =
          !profileInfo.isSupervised || profileInfo.isChild;

      // Because this dialog isn't useful when refreshing or as part of the
      // history, don't create a history entry for it when showing.
      PageManager.showPageByName('manageProfile', false);
      chrome.send('logDeleteUserDialogShown');
    },

    /**
     * Display the "Disconnect Managed Profile" dialog.
     * @private
     */
    showDisconnectManagedProfileDialog_: function(replacements) {
      loadTimeData.overrideValues(replacements);
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = true;
      $('disconnect-managed-profile-domain-information').innerHTML =
          loadTimeData.getString('disconnectManagedProfileDomainInformation');
      $('disconnect-managed-profile-text').innerHTML =
          loadTimeData.getString('disconnectManagedProfileText');
      $('manage-profile-overlay-disconnect-managed').hidden = false;

      PageManager.showPageByName('signOut');
    },

    /**
     * Display the "Create Profile" dialog.
     * @private
     */
    showCreateDialog_: function() {
      PageManager.showPageByName('createProfile');
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(ManageProfileOverlay, [
    'receiveDefaultProfileIconsAndNames',
    'receiveNewProfileDefaults',
    'receiveExistingProfileNames',
    'receiveHasProfileShortcuts',
    'setProfileInfo',
    'setProfileName',
    'showManageDialog',
    'showDeleteDialog',
    'showDisconnectManagedProfileDialog',
    'showCreateDialog',
  ]);

  /**
   * @constructor
   * @extends {options.ManageProfileOverlay}
   */
  function DisconnectAccountOverlay() {
    Page.call(this, 'signOut',
              loadTimeData.getString('disconnectAccountTabTitle'),
              'manage-profile-overlay');
  }

  cr.addSingletonGetter(DisconnectAccountOverlay);

  DisconnectAccountOverlay.prototype = {
    __proto__: ManageProfileOverlay.prototype,

    /** @override */
    canShowPage: function() {
      var syncData = loadTimeData.getValue('syncData');
      return syncData.signedIn && !syncData.signoutAllowed;
    },

    /** @override */
    didShowPage: function() {
      chrome.send('showDisconnectManagedProfileDialog');
    }
  };

  /**
   * @constructor
   * @extends {options.ManageProfileOverlay}
   */
  function CreateProfileOverlay() {
    Page.call(this, 'createProfile',
              loadTimeData.getString('createProfileTabTitle'),
              'manage-profile-overlay');
  }

  cr.addSingletonGetter(CreateProfileOverlay);

  CreateProfileOverlay.prototype = {
    __proto__: ManageProfileOverlay.prototype,

    // The signed-in email address of the current profile, or empty if they're
    // not signed in.
    signedInEmail_: '',

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isSupervised;
    },

    /**
     * Configures the overlay to the "create user" mode.
     * @override
     */
    didShowPage: function() {
      chrome.send('requestCreateProfileUpdate');
      chrome.send('requestDefaultProfileIcons', ['create']);
      chrome.send('requestNewProfileDefaults');

      $('manage-profile-overlay-create').hidden = false;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = true;
      $('manage-profile-overlay-disconnect-managed').hidden = true;
      $('create-profile-instructions').textContent =
         loadTimeData.getStringF('createProfileInstructions');
      this.hideErrorBubble_();
      this.updateCreateInProgress_(false);

      var shortcutsEnabled = loadTimeData.getBoolean('profileShortcutsEnabled');
      $('create-shortcut-container').hidden = !shortcutsEnabled;
      $('create-shortcut').checked = shortcutsEnabled;

      $('create-profile-name-label').hidden = true;
      $('create-profile-name').hidden = true;
      $('create-profile-ok').disabled = true;

      $('create-profile-supervised').checked = false;
      $('import-existing-supervised-user-link').hidden = true;
      $('create-profile-supervised').onchange = function() {
        ManageProfileOverlay.getInstance().updateCreateOrImport_('create');
      };
      $('create-profile-supervised').hidden = true;
      $('create-profile-supervised-signed-in').disabled = true;
      $('create-profile-supervised-signed-in').hidden = true;
      $('create-profile-supervised-not-signed-in').hidden = true;

      this.profileNameIsDefault_ = false;
    },

    /** @override */
    handleCancel: function() {
      this.cancelCreateProfile_();
    },

    /** @override */
    showErrorBubble_: function(errorHtml) {
      ManageProfileOverlay.getInstance().showErrorBubble_(errorHtml,
                                                          'create',
                                                          false);
    },

    /** @override */
    hideErrorBubble_: function() {
      ManageProfileOverlay.getInstance().hideErrorBubble_('create');
    },

    /**
     * Updates the UI when a profile create step begins or ends.
     * Note that hideErrorBubble_() also enables the "OK" button, so it
     * must be called before this function if both are used.
     * @param {boolean} inProgress True if the UI should be updated to show that
     *     profile creation is now in progress.
     * @private
     */
    updateCreateInProgress_: function(inProgress) {
      this.createInProgress_ = inProgress;
      this.updateCreateSupervisedUserCheckbox_();

      $('create-profile-icon-grid').disabled = inProgress;
      $('create-profile-name').disabled = inProgress;
      $('create-shortcut').disabled = inProgress;
      $('create-profile-ok').disabled = inProgress;
      $('import-existing-supervised-user-link').disabled = inProgress;

      $('create-profile-throbber').hidden = !inProgress;
    },

    /**
     * Cancels the creation of the a profile. It is safe to call this even
     * when no profile is in the process of being created.
     * @private
     */
    cancelCreateProfile_: function() {
      PageManager.closeOverlay();
      chrome.send('cancelCreateProfile');
      this.hideErrorBubble_();
      this.updateCreateInProgress_(false);
    },

    /**
     * Shows an error message describing an error that occurred while creating
     * a new profile.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} error The error message to display.
     * @private
     */
    onError_: function(error) {
      this.updateCreateInProgress_(false);
      this.showErrorBubble_(error);
    },

    /**
     * Shows a warning message giving information while creating a new profile.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} warning The warning message to display.
     * @private
     */
    onWarning_: function(warning) {
      this.showErrorBubble_(warning);
    },

    /**
     * For new supervised users, shows a confirmation page after successfully
     * creating a new profile; otherwise, the handler will open a new window.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isSupervised: (true|false),
     *     };
     * @private
     */
    onSuccess_: function(profileInfo) {
      this.updateCreateInProgress_(false);
      PageManager.closeOverlay();
      if (profileInfo.isSupervised) {
        options.SupervisedUserListData.resetPromise();
        profileInfo.custodianEmail = this.signedInEmail_;
        SupervisedUserCreateConfirmOverlay.setProfileInfo(profileInfo);
        PageManager.showPageByName('supervisedUserCreateConfirm', false);
        BrowserOptions.updateManagesSupervisedUsers(true);
      }
    },

    /**
     * @param {string} email
     * @param {boolean} hasError
     * @override
     */
    updateSignedInStatus: function(email, hasError) {
      this.updateSignedInStatus_(email, hasError);
    },

    /**
     * Updates the signed-in or not-signed-in UI when in create mode. Called by
     * the handler in response to the 'requestCreateProfileUpdate' message.
     * updateSupervisedUsersAllowed_ is expected to be called after this is, and
     * will update additional UI elements.
     * @param {string} email The email address of the currently signed-in user.
     *     An empty string indicates that the user is not signed in.
     * @param {boolean} hasError Whether the user's sign-in credentials are
     *     still valid.
     * @private
     */
    updateSignedInStatus_: function(email, hasError) {
      this.signedInEmail_ = email;
      this.hasError_ = hasError;
      var isSignedIn = email !== '';
      $('create-profile-supervised').hidden = !isSignedIn;
      $('create-profile-supervised-signed-in').hidden = !isSignedIn;
      $('create-profile-supervised-not-signed-in').hidden = isSignedIn;

      if (isSignedIn) {
        var accountDetailsOutOfDate =
            $('create-profile-supervised-account-details-out-of-date-label');
        accountDetailsOutOfDate.textContent = loadTimeData.getStringF(
            'manageProfilesSupervisedAccountDetailsOutOfDate', email);
        accountDetailsOutOfDate.hidden = !hasError;

        $('create-profile-supervised-signed-in-label').textContent =
            loadTimeData.getStringF(
                'manageProfilesSupervisedSignedInLabel', email);
        $('create-profile-supervised-signed-in-label').hidden = hasError;

        $('create-profile-supervised-sign-in-again-link').hidden = !hasError;
        $('create-profile-supervised-signed-in-learn-more-link').hidden =
            hasError;
      }

      this.updateCreateSupervisedUserCheckbox_();
      // If we're signed in, showing/hiding import-existing-supervised-user-link
      // is handled in receiveExistingSupervisedUsers_.
      if (isSignedIn && !hasError)
        this.requestExistingSupervisedUsers_();
      else
        $('import-existing-supervised-user-link').hidden = true;
    },

    /**
     * Sets whether creating supervised users is allowed or not. Called by the
     * handler in response to the 'requestCreateProfileUpdate' message or a
     * change in the (policy-controlled) pref that prohibits creating supervised
     * users, after the signed-in status has been updated.
     * @param {boolean} allowed True if creating supervised users should be
     *     allowed.
     * @private
     */
    updateSupervisedUsersAllowed_: function(allowed) {
      this.supervisedUsersAllowed_ = allowed;
      this.updateCreateSupervisedUserCheckbox_();

      $('create-profile-supervised-sign-in-link').enabled = allowed;
      if (!allowed) {
        $('create-profile-supervised-indicator').setAttribute('controlled-by',
                                                              'policy');
      } else {
        $('create-profile-supervised-indicator').removeAttribute(
            'controlled-by');
      }
    },

    /**
     * Updates the status of the "create supervised user" checkbox. Called from
     * updateSupervisedUsersAllowed_() or updateCreateInProgress_().
     * updateSignedInStatus_() does not call this method directly, because it
     * will be followed by a call to updateSupervisedUsersAllowed_().
     * @private
     */
    updateCreateSupervisedUserCheckbox_: function() {
      $('create-profile-supervised').disabled =
          !this.supervisedUsersAllowed_ || this.createInProgress_ ||
          this.signedInEmail_ == '' || this.hasError_;
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(CreateProfileOverlay, [
    'cancelCreateProfile',
    'onError',
    'onSuccess',
    'onWarning',
    'updateCreateInProgress',
    'updateSignedInStatus',
    'updateSupervisedUsersAllowed',
  ]);

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay,
    DisconnectAccountOverlay: DisconnectAccountOverlay,
    CreateProfileOverlay: CreateProfileOverlay,
  };
});
