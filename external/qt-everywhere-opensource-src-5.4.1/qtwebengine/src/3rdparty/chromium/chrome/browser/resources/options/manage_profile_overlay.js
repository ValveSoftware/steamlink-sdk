// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * ManageProfileOverlay class
   * Encapsulated handling of the 'Manage profile...' overlay page.
   * @constructor
   * @class
   */
  function ManageProfileOverlay() {
    OptionsPage.call(this, 'manageProfile',
                     loadTimeData.getString('manageProfileTabTitle'),
                     'manage-profile-overlay');
  };

  cr.addSingletonGetter(ManageProfileOverlay);

  ManageProfileOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

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

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

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
        OptionsPage.closeOverlay();
      };
      $('delete-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        if (BrowserOptions.getCurrentProfile().isManaged)
          return;
        chrome.send('deleteProfile', [self.profileInfo_.filePath]);
        options.ManagedUserListData.resetPromise();
      };
      $('add-shortcut-button').onclick = function(event) {
        chrome.send('addProfileShortcut', [self.profileInfo_.filePath]);
      };
      $('remove-shortcut-button').onclick = function(event) {
        chrome.send('removeProfileShortcut', [self.profileInfo_.filePath]);
      };

      $('disconnect-managed-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('deleteProfile',
                    [BrowserOptions.getCurrentProfile().filePath]);
      }

      $('create-profile-managed-signed-in-learn-more-link').onclick =
          function(event) {
        OptionsPage.navigateToPage('managedUserLearnMore');
        return false;
      };

      $('create-profile-managed-not-signed-in-link').onclick = function(event) {
        // The signin process will open an overlay to configure sync, which
        // would replace this overlay. It's smoother to close this one now.
        // TODO(pamg): Move the sync-setup overlay to a higher layer so this one
        // can stay open under it, after making sure that doesn't break anything
        // else.
        OptionsPage.closeOverlay();
        SyncSetupOverlay.startSignIn();
      };

      $('create-profile-managed-sign-in-again-link').onclick = function(event) {
        OptionsPage.closeOverlay();
        SyncSetupOverlay.showSetupUI();
      };

      $('import-existing-managed-user-link').onclick = function(event) {
        // Hide the import button to trigger a cursor update. The import button
        // is shown again when the import overlay loads. TODO(akuegel): Remove
        // this temporary fix when crbug/246304 is resolved.
        $('import-existing-managed-user-link').hidden = true;
        OptionsPage.navigateToPage('managedUserImport');
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
      // Supervised users cannot edit their names.
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
        OptionsPage.closeOverlay();
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
     *       isManaged: false
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
     * @param {Array.<string>} iconURLs An array of icon URLs.
     * @param {Array.<string>} names An array of default names
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
      if (OptionsPage.getTopmostVisiblePage().pageDiv == this.pageDiv) {
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
     * Called when the profile name is changed or the 'create managed' checkbox
     * is toggled. Updates the 'ok' button and the 'import existing supervised
     * user' link.
     * @param {string} mode A label that specifies the type of dialog box which
     *     is currently being viewed (i.e. 'create' or 'manage').
     * @private
     */
    updateCreateOrImport_: function(mode) {
      // In 'create' mode, check for existing managed users with the same name.
      if (mode == 'create' && $('create-profile-managed').checked) {
        options.ManagedUserListData.requestExistingManagedUsers().then(
            this.receiveExistingManagedUsers_.bind(this),
            this.onSigninError_.bind(this));
      } else {
        this.updateOkButton_(mode);
      }
    },

    /**
     * Callback which receives the list of existing managed users. Checks if the
     * currently entered name is the name of an already existing managed user.
     * If yes, the user is prompted to import the existing managed user, and the
     * create button is disabled.
     * @param {Array.<Object>} The list of existing managed users.
     * @private
     */
    receiveExistingManagedUsers_: function(managedUsers) {
      var newName = $('create-profile-name').value;
      var i;
      for (i = 0; i < managedUsers.length; ++i) {
        if (managedUsers[i].name == newName &&
            !managedUsers[i].onCurrentDevice) {
          var errorHtml = loadTimeData.getStringF(
              'manageProfilesExistingSupervisedUser',
              HTMLEscape(elide(newName, /* maxLength */ 50)));
          this.showErrorBubble_(errorHtml, 'create', true);

          // Check if another supervised user also exists with that name.
          var nameIsUnique = true;
          var j;
          for (j = i + 1; j < managedUsers.length; ++j) {
            if (managedUsers[j].name == newName) {
              nameIsUnique = false;
              break;
            }
          }
          var self = this;
          function getImportHandler(managedUser, nameIsUnique) {
            return function() {
              if (managedUser.needAvatar || !nameIsUnique) {
                OptionsPage.navigateToPage('managedUserImport');
              } else {
                self.hideErrorBubble_('create');
                CreateProfileOverlay.updateCreateInProgress(true);
                chrome.send('createProfile',
                    [managedUser.name, managedUser.iconURL, false, true,
                        managedUser.id]);
              }
            }
          };
          $('supervised-user-import').onclick =
              getImportHandler(managedUsers[i], nameIsUnique);
          $('create-profile-ok').disabled = true;
          return;
        }
      }
      this.updateOkButton_('create');
    },

    /**
     * Called in case the request for the list of managed users fails because of
     * a signin error.
     * @private
     */
    onSigninError_: function() {
      this.updateImportExistingManagedUserLink_(false);
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
      var nameIsDuplicate = this.existingProfileNames_[newName] != undefined;
      if (mode == 'manage' && oldName == newName)
        nameIsDuplicate = false;
      if (nameIsDuplicate) {
        var errorHtml =
            loadTimeData.getString('manageProfilesDuplicateNameError');
        this.showErrorBubble_(errorHtml, mode, true);
      } else {
        this.hideErrorBubble_(mode);

        var nameIsValid = $(mode + '-profile-name').validity.valid;
        $(mode + '-profile-ok').disabled = !nameIsValid;
      }
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
        options.ManagedUserListData.resetPromise();
    },

    /**
     * Called when the user clicks "OK" or hits enter. Creates the profile
     * using the information in the dialog.
     * @private
     */
    submitCreateProfile_: function() {
      // This is visual polish: the UI to access this should be disabled for
      // managed users, and the back end will prevent user creation anyway.
      if (this.profileInfo_ && this.profileInfo_.isManaged)
        return;

      this.hideErrorBubble_('create');
      CreateProfileOverlay.updateCreateInProgress(true);

      // Get the user's chosen name and icon, or default if they do not
      // wish to customize their profile.
      var name = $('create-profile-name').value;
      var iconUrl = $('create-profile-icon-grid').selectedItem;
      var createShortcut = $('create-shortcut').checked;
      var isManaged = $('create-profile-managed').checked;
      var existingManagedUserId = '';

      // 'createProfile' is handled by the CreateProfileHandler.
      chrome.send('createProfile',
                  [name, iconUrl, createShortcut,
                   isManaged, existingManagedUserId]);
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
      $('manage-profile-name').disabled = profileInfo.isManaged;
      this.hideErrorBubble_('manage');
    },

    /**
     * Display the "Manage Profile" dialog.
     * @private
     */
    showManageDialog_: function() {
      this.prepareForManageDialog_();
      OptionsPage.navigateToPage('manageProfile');
    },

    /**
     * Display the "Delete Profile" dialog.
     * @param {Object} profileInfo The profile object of the profile to delete.
     * @private
     */
    showDeleteDialog_: function(profileInfo) {
      if (BrowserOptions.getCurrentProfile().isManaged)
        return;

      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = false;
      $('manage-profile-overlay-disconnect-managed').hidden = true;
      $('delete-profile-icon').style.content =
          getProfileAvatarIcon(profileInfo.iconURL);
      $('delete-profile-text').textContent =
          loadTimeData.getStringF('deleteProfileMessage',
                                  elide(profileInfo.name, /* maxLength */ 50));
      $('delete-managed-profile-addendum').hidden = !profileInfo.isManaged;

      // Because this dialog isn't useful when refreshing or as part of the
      // history, don't create a history entry for it when showing.
      OptionsPage.showPageByName('manageProfile', false);
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

      // Because this dialog isn't useful when refreshing or as part of the
      // history, don't create a history entry for it when showing.
      OptionsPage.showPageByName('manageProfile', false);
    },

    /**
     * Display the "Create Profile" dialog.
     * @private
     */
    showCreateDialog_: function() {
      OptionsPage.navigateToPage('createProfile');
    },
  };

  // Forward public APIs to private implementations.
  [
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
  ].forEach(function(name) {
    ManageProfileOverlay[name] = function() {
      var instance = ManageProfileOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  function CreateProfileOverlay() {
    OptionsPage.call(this, 'createProfile',
                     loadTimeData.getString('createProfileTabTitle'),
                     'manage-profile-overlay');
  };

  cr.addSingletonGetter(CreateProfileOverlay);

  CreateProfileOverlay.prototype = {
    // Inherit from ManageProfileOverlay.
    __proto__: ManageProfileOverlay.prototype,

    // The signed-in email address of the current profile, or empty if they're
    // not signed in.
    signedInEmail_: '',

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isManaged;
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

      $('create-profile-managed').checked = false;
      $('import-existing-managed-user-link').hidden = true;
      $('create-profile-managed').onchange = function() {
        ManageProfileOverlay.getInstance().updateCreateOrImport_('create');
      };
      $('create-profile-managed-signed-in').disabled = true;
      $('create-profile-managed-signed-in').hidden = true;
      $('create-profile-managed-not-signed-in').hidden = true;

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
      this.updateCreateManagedUserCheckbox_();

      $('create-profile-icon-grid').disabled = inProgress;
      $('create-profile-name').disabled = inProgress;
      $('create-shortcut').disabled = inProgress;
      $('create-profile-ok').disabled = inProgress;
      $('import-existing-managed-user-link').disabled = inProgress;

      $('create-profile-throbber').hidden = !inProgress;
    },

    /**
     * Cancels the creation of the a profile. It is safe to call this even
     * when no profile is in the process of being created.
     * @private
     */
    cancelCreateProfile_: function() {
      OptionsPage.closeOverlay();
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
     *       isManaged: (true|false),
     *     };
     * @private
     */
    onSuccess_: function(profileInfo) {
      this.updateCreateInProgress_(false);
      OptionsPage.closeOverlay();
      if (profileInfo.isManaged) {
        options.ManagedUserListData.resetPromise();
        profileInfo.custodianEmail = this.signedInEmail_;
        ManagedUserCreateConfirmOverlay.setProfileInfo(profileInfo);
        OptionsPage.showPageByName('managedUserCreateConfirm', false);
        BrowserOptions.updateManagesSupervisedUsers(true);
      }
    },

    /**
     * Updates the signed-in or not-signed-in UI when in create mode. Called by
     * the handler in response to the 'requestCreateProfileUpdate' message.
     * updateManagedUsersAllowed_ is expected to be called after this is, and
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
      $('create-profile-managed-signed-in').hidden = !isSignedIn;
      $('create-profile-managed-not-signed-in').hidden = isSignedIn;

      if (isSignedIn) {
        var accountDetailsOutOfDate =
            $('create-profile-managed-account-details-out-of-date-label');
        accountDetailsOutOfDate.textContent = loadTimeData.getStringF(
            'manageProfilesManagedAccountDetailsOutOfDate', email);
        accountDetailsOutOfDate.hidden = !hasError;

        $('create-profile-managed-signed-in-label').textContent =
            loadTimeData.getStringF(
                'manageProfilesManagedSignedInLabel', email);
        $('create-profile-managed-signed-in-label').hidden = hasError;

        $('create-profile-managed-sign-in-again-link').hidden = !hasError;
        $('create-profile-managed-signed-in-learn-more-link').hidden = hasError;
      }

      this.updateImportExistingManagedUserLink_(isSignedIn && !hasError);
    },

    /**
     * Enables/disables the 'import existing managed users' link button.
     * It also updates the button text.
     * @param {boolean} enable True to enable the link button and
     *     false otherwise.
     * @private
     */
    updateImportExistingManagedUserLink_: function(enable) {
      var importManagedUserElement = $('import-existing-managed-user-link');
      importManagedUserElement.hidden = false;
      importManagedUserElement.disabled = !enable || this.createInProgress_;
      importManagedUserElement.textContent = enable ?
          loadTimeData.getString('importExistingManagedUserLink') :
          loadTimeData.getString('signInToImportManagedUsers');
    },

    /**
     * Sets whether creating managed users is allowed or not. Called by the
     * handler in response to the 'requestCreateProfileUpdate' message or a
     * change in the (policy-controlled) pref that prohibits creating managed
     * users, after the signed-in status has been updated.
     * @param {boolean} allowed True if creating managed users should be
     *     allowed.
     * @private
     */
    updateManagedUsersAllowed_: function(allowed) {
      this.managedUsersAllowed_ = allowed;
      this.updateCreateManagedUserCheckbox_();

      $('create-profile-managed-not-signed-in-link').hidden = !allowed;
      if (!allowed) {
        $('create-profile-managed-indicator').setAttribute('controlled-by',
                                                           'policy');
      } else {
        $('create-profile-managed-indicator').removeAttribute('controlled-by');
      }
    },

    /**
     * Updates the status of the "create managed user" checkbox. Called from
     * updateManagedUsersAllowed_() or updateCreateInProgress_().
     * updateSignedInStatus_() does not call this method directly, because it
     * will be followed by a call to updateManagedUsersAllowed_().
     * @private
     */
    updateCreateManagedUserCheckbox_: function() {
      $('create-profile-managed').disabled =
          !this.managedUsersAllowed_ || this.createInProgress_ ||
          this.signedInEmail_ == '' || this.hasError_;
    },
  };

  // Forward public APIs to private implementations.
  [
    'cancelCreateProfile',
    'onError',
    'onSuccess',
    'onWarning',
    'updateCreateInProgress',
    'updateManagedUsersAllowed',
    'updateSignedInStatus',
  ].forEach(function(name) {
    CreateProfileOverlay[name] = function() {
      var instance = CreateProfileOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay,
    CreateProfileOverlay: CreateProfileOverlay,
  };
});
