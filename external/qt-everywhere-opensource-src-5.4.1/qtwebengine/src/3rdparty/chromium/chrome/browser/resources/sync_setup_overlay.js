// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  // True if the synced account uses a custom passphrase.
  var usePassphrase_ = false;

  // True if the synced account uses 'encrypt everything'.
  var useEncryptEverything_ = false;

  // An object used as a cache of the arguments passed in while initially
  // displaying the advanced sync settings dialog. Used to switch between the
  // options in the main drop-down menu. Reset when the dialog is closed.
  var syncConfigureArgs_ = null;

  // A dictionary that maps the sync data type checkbox names to their checked
  // state. Initialized when the advanced settings dialog is first brought up,
  // updated any time a box is checked / unchecked, and reset when the dialog is
  // closed. Used to restore checkbox state while switching between the options
  // in the main drop-down menu. All checkboxes are checked and disabled when
  // the "Sync everything" menu-item is selected, and unchecked and disabled
  // when "Sync nothing" is selected. When "Choose what to sync" is selected,
  // the boxes are restored to their most recent checked state from this cache.
  var dataTypeBoxes_ = {};

  // Used to determine whether to bring the OK button / passphrase field into
  // focus.
  var confirmPageVisible_ = false;
  var customizePageVisible_ = false;

  /**
   * The user's selection in the synced data type drop-down menu, as an index.
   * @enum {number}
   * @const
   */
  var DataTypeSelection = {
    SYNC_EVERYTHING: 0,
    CHOOSE_WHAT_TO_SYNC: 1,
    SYNC_NOTHING: 2
  };

  /**
   * SyncSetupOverlay class
   * Encapsulated handling of the 'Sync Setup' overlay page.
   * @class
   */
  function SyncSetupOverlay() {
    OptionsPage.call(this, 'syncSetup',
                     loadTimeData.getString('syncSetupOverlayTabTitle'),
                     'sync-setup-overlay');
  }

  cr.addSingletonGetter(SyncSetupOverlay);

  SyncSetupOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('basic-encryption-option').onchange =
          $('full-encryption-option').onchange = function() {
        self.onEncryptionRadioChanged_();
      }
      $('choose-datatypes-cancel').onclick =
          $('confirm-everything-cancel').onclick =
          $('stop-syncing-cancel').onclick =
          $('sync-spinner-cancel').onclick = function() {
        self.closeOverlay_();
      };
      $('confirm-everything-ok').onclick = function() {
        self.sendConfiguration_();
      };
      $('timeout-ok').onclick = function() {
        chrome.send('CloseTimeout');
        self.closeOverlay_();
      };
      $('stop-syncing-ok').onclick = function() {
        var deleteProfile = $('delete-profile') != undefined &&
            $('delete-profile').checked;
        chrome.send('SyncSetupStopSyncing', [deleteProfile]);
        self.closeOverlay_();
      };
    },

    showOverlay_: function() {
      OptionsPage.navigateToPage('syncSetup');
    },

    closeOverlay_: function() {
      this.syncConfigureArgs_ = null;
      this.dataTypeBoxes_ = {};

      var overlay = $('sync-setup-overlay');
      if (!overlay.hidden)
        OptionsPage.closeOverlay();
    },

    /** @override */
    didShowPage: function() {
      chrome.send('SyncSetupShowSetupUI');
    },

    /** @override */
    didClosePage: function() {
      chrome.send('SyncSetupDidClosePage');
    },

    onEncryptionRadioChanged_: function() {
      var visible = $('full-encryption-option').checked;
      $('sync-custom-passphrase').hidden = !visible;
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_SyncSetEncryption']);
    },

    /**
     * Sets the checked state of the individual sync data type checkboxes in the
     * advanced sync settings dialog.
     * @param {boolean} value True for checked, false for unchecked.
     * @private
     */
    checkAllDataTypeCheckboxes_: function(value) {
      // Only check / uncheck the visible ones (since there's no way to uncheck
      // / check the invisible ones).
      var checkboxes = $('choose-data-types-body').querySelectorAll(
          '.sync-type-checkbox:not([hidden]) input');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].checked = value;
      }
    },

    /**
     * Restores the checked states of the sync data type checkboxes in the
     * advanced sync settings dialog. Called when "Choose what to sync" is
     * selected. Required because all the checkboxes are checked when
     * "Sync everything" is selected, and unchecked when "Sync nothing" is
     * selected. Note: We only restore checkboxes for data types that are
     * actually visible and whose old values are found in the cache, since it's
     * possible for some data types to not be registered, and therefore, their
     * checkboxes remain hidden, and never get cached.
     * @private
     */
    restoreDataTypeCheckboxes_: function() {
      for (dataType in dataTypeBoxes_) {
        $(dataType).checked = dataTypeBoxes_[dataType];
      }
    },

    /**
     * Enables / grays out the sync data type checkboxes in the advanced
     * settings dialog.
     * @param {boolean} enabled True for enabled, false for grayed out.
     * @private
     */
    setDataTypeCheckboxesEnabled_: function(enabled) {
      var checkboxes = $('choose-data-types-body').querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].disabled = !enabled;
      }
    },

    /**
     * Sets the state of the sync data type checkboxes based on whether "Sync
     * everything", "Choose what to sync", or "Sync nothing" are selected in the
     * drop-down menu of the advanced settings dialog.
     * @param {cr.DataTypeSelection} selectedIndex Index of user's selection.
     * @private
     */
    setDataTypeCheckboxes_: function(selectedIndex) {
      if (selectedIndex == DataTypeSelection.CHOOSE_WHAT_TO_SYNC) {
        this.setDataTypeCheckboxesEnabled_(true);
        this.restoreDataTypeCheckboxes_();
      } else {
        this.setDataTypeCheckboxesEnabled_(false);
        this.checkAllDataTypeCheckboxes_(selectedIndex ==
                                         DataTypeSelection.SYNC_EVERYTHING);
      }
    },

    checkPassphraseMatch_: function() {
      var emptyError = $('empty-error');
      var mismatchError = $('mismatch-error');
      emptyError.hidden = true;
      mismatchError.hidden = true;

      var f = $('choose-data-types-form');
      if (!$('full-encryption-option').checked ||
           $('basic-encryption-option').disabled) {
        return true;
      }

      var customPassphrase = $('custom-passphrase');
      if (customPassphrase.value.length == 0) {
        emptyError.hidden = false;
        return false;
      }

      var confirmPassphrase = $('confirm-passphrase');
      if (confirmPassphrase.value != customPassphrase.value) {
        mismatchError.hidden = false;
        return false;
      }

      return true;
    },

    sendConfiguration_: function() {
      var encryptAllData = $('full-encryption-option').checked;

      var usePassphrase;
      var customPassphrase;
      var googlePassphrase = false;
      if (!$('sync-existing-passphrase-container').hidden) {
        // If we were prompted for an existing passphrase, use it.
        customPassphrase = $('choose-data-types-form').passphrase.value;
        usePassphrase = true;
        // If we were displaying the 'enter your old google password' prompt,
        // then that means this is the user's google password.
        googlePassphrase = !$('google-passphrase-needed-body').hidden;
        // We allow an empty passphrase, in case the user has disabled
        // all their encrypted datatypes. In that case, the PSS will accept
        // the passphrase and finish configuration. If the user has enabled
        // encrypted datatypes, the PSS will prompt again specifying that the
        // passphrase failed.
      } else if (!$('basic-encryption-option').disabled &&
                  $('full-encryption-option').checked) {
        // The user is setting a custom passphrase for the first time.
        if (!this.checkPassphraseMatch_())
          return;
        customPassphrase = $('custom-passphrase').value;
        usePassphrase = true;
      } else {
        // The user is not setting a custom passphrase.
        usePassphrase = false;
      }

      // Don't allow the user to tweak the settings once we send the
      // configuration to the backend.
      this.setInputElementsDisabledState_(true);
      $('use-default-link').hidden = true;
      $('use-default-link').disabled = true;
      $('use-default-link').onclick = null;

      // These values need to be kept in sync with where they are read in
      // SyncSetupFlow::GetDataTypeChoiceData().
      var syncAll = $('sync-select-datatypes').selectedIndex ==
                    DataTypeSelection.SYNC_EVERYTHING;
      var syncNothing = $('sync-select-datatypes').selectedIndex ==
                        DataTypeSelection.SYNC_NOTHING;
      var result = JSON.stringify({
        'syncAllDataTypes': syncAll,
        'syncNothing': syncNothing,
        'bookmarksSynced': syncAll || $('bookmarks-checkbox').checked,
        'preferencesSynced': syncAll || $('preferences-checkbox').checked,
        'themesSynced': syncAll || $('themes-checkbox').checked,
        'passwordsSynced': syncAll || $('passwords-checkbox').checked,
        'autofillSynced': syncAll || $('autofill-checkbox').checked,
        'extensionsSynced': syncAll || $('extensions-checkbox').checked,
        'typedUrlsSynced': syncAll || $('typed-urls-checkbox').checked,
        'appsSynced': syncAll || $('apps-checkbox').checked,
        'tabsSynced': syncAll || $('tabs-checkbox').checked,
        'encryptAllData': encryptAllData,
        'usePassphrase': usePassphrase,
        'isGooglePassphrase': googlePassphrase,
        'passphrase': customPassphrase
      });
      chrome.send('SyncSetupConfigure', [result]);
    },

    /**
     * Sets the disabled property of all input elements within the 'Customize
     * Sync Preferences' screen. This is used to prohibit the user from changing
     * the inputs after confirming the customized sync preferences, or resetting
     * the state when re-showing the dialog.
     * @param {boolean} disabled True if controls should be set to disabled.
     * @private
     */
    setInputElementsDisabledState_: function(disabled) {
      var configureElements =
          $('customize-sync-preferences').querySelectorAll('input');
      for (var i = 0; i < configureElements.length; i++)
        configureElements[i].disabled = disabled;
      $('sync-select-datatypes').disabled = disabled;

      $('customize-link').hidden = disabled;
      $('customize-link').disabled = disabled;
      $('customize-link').onclick = (disabled ? null : function() {
        SyncSetupOverlay.showCustomizePage(null,
                                           DataTypeSelection.SYNC_EVERYTHING);
        return false;
      });
    },

    /**
     * Shows or hides the sync data type checkboxes in the advanced sync
     * settings dialog. Also initializes |dataTypeBoxes_| with their values, and
     * makes their onclick handlers update |dataTypeBoxes_|.
     * @param {Object} args The configuration data used to show/hide UI.
     * @private
     */
    setChooseDataTypesCheckboxes_: function(args) {
      var datatypeSelect = $('sync-select-datatypes');
      datatypeSelect.selectedIndex = args.syncAllDataTypes ?
                                         DataTypeSelection.SYNC_EVERYTHING :
                                         DataTypeSelection.CHOOSE_WHAT_TO_SYNC;

      $('bookmarks-checkbox').checked = args.bookmarksSynced;
      dataTypeBoxes_['bookmarks-checkbox'] = args.bookmarksSynced;
      $('bookmarks-checkbox').onclick = this.handleDataTypeClick_;

      $('preferences-checkbox').checked = args.preferencesSynced;
      dataTypeBoxes_['preferences-checkbox'] = args.preferencesSynced;
      $('preferences-checkbox').onclick = this.handleDataTypeClick_;

      $('themes-checkbox').checked = args.themesSynced;
      dataTypeBoxes_['themes-checkbox'] = args.themesSynced;
      $('themes-checkbox').onclick = this.handleDataTypeClick_;

      if (args.passwordsRegistered) {
        $('passwords-checkbox').checked = args.passwordsSynced;
        dataTypeBoxes_['passwords-checkbox'] = args.passwordsSynced;
        $('passwords-checkbox').onclick = this.handleDataTypeClick_;
        $('passwords-item').hidden = false;
      } else {
        $('passwords-item').hidden = true;
      }
      if (args.autofillRegistered) {
        $('autofill-checkbox').checked = args.autofillSynced;
        dataTypeBoxes_['autofill-checkbox'] = args.autofillSynced;
        $('autofill-checkbox').onclick = this.handleDataTypeClick_;
        $('autofill-item').hidden = false;
      } else {
        $('autofill-item').hidden = true;
      }
      if (args.extensionsRegistered) {
        $('extensions-checkbox').checked = args.extensionsSynced;
        dataTypeBoxes_['extensions-checkbox'] = args.extensionsSynced;
        $('extensions-checkbox').onclick = this.handleDataTypeClick_;
        $('extensions-item').hidden = false;
      } else {
        $('extensions-item').hidden = true;
      }
      if (args.typedUrlsRegistered) {
        $('typed-urls-checkbox').checked = args.typedUrlsSynced;
        dataTypeBoxes_['typed-urls-checkbox'] = args.typedUrlsSynced;
        $('typed-urls-checkbox').onclick = this.handleDataTypeClick_;
        $('omnibox-item').hidden = false;
      } else {
        $('omnibox-item').hidden = true;
      }
      if (args.appsRegistered) {
        $('apps-checkbox').checked = args.appsSynced;
        dataTypeBoxes_['apps-checkbox'] = args.appsSynced;
        $('apps-checkbox').onclick = this.handleDataTypeClick_;
        $('apps-item').hidden = false;
      } else {
        $('apps-item').hidden = true;
      }
      if (args.tabsRegistered) {
        $('tabs-checkbox').checked = args.tabsSynced;
        dataTypeBoxes_['tabs-checkbox'] = args.tabsSynced;
        $('tabs-checkbox').onclick = this.handleDataTypeClick_;
        $('tabs-item').hidden = false;
      } else {
        $('tabs-item').hidden = true;
      }

      this.setDataTypeCheckboxes_(datatypeSelect.selectedIndex);
    },

    /**
     * Updates the cached values of the sync data type checkboxes stored in
     * |dataTypeBoxes_|. Used as an onclick handler for each data type checkbox.
     * @private
     */
    handleDataTypeClick_: function() {
      dataTypeBoxes_[this.id] = this.checked;
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_SyncToggleDataType']);
    },

    setEncryptionRadios_: function(args) {
      if (!args.encryptAllData && !args.usePassphrase) {
        $('basic-encryption-option').checked = true;
      } else {
        $('full-encryption-option').checked = true;
        $('full-encryption-option').disabled = true;
        $('basic-encryption-option').disabled = true;
      }
    },

    setCheckboxesAndErrors_: function(args) {
      this.setChooseDataTypesCheckboxes_(args);
      this.setEncryptionRadios_(args);
    },

    showConfigure_: function(args) {
      var datatypeSelect = $('sync-select-datatypes');
      var self = this;

      // Cache the sync config args so they can be reused when we transition
      // between the drop-down menu items in the advanced settings dialog.
      if (args)
        this.syncConfigureArgs_ = args;

      // Required in order to determine whether to give focus to the OK button
      // or passphrase field. See crbug.com/310555 and crbug.com/306353.
      this.confirmPageVisible_ = false;
      this.customizePageVisible_ = false;

      // Once the advanced sync settings dialog is visible, we transition
      // between its drop-down menu items as follows:
      // "Sync everything": Show encryption and passphrase sections, and disable
      // and check all data type checkboxes.
      // "Sync nothing": Hide encryption and passphrase sections, and disable
      // and uncheck all data type checkboxes.
      // "Choose what to sync": Show encryption and passphrase sections, enable
      // data type checkboxes, and restore their checked state to the last time
      // the "Choose what to sync" was selected while the dialog was still up.
      datatypeSelect.onchange = function() {
        if (this.selectedIndex == DataTypeSelection.SYNC_NOTHING) {
          self.showSyncNothingPage_();
        } else {
          self.showCustomizePage_(self.syncConfigureArgs_, this.selectedIndex);
          if (this.selectedIndex == DataTypeSelection.SYNC_EVERYTHING)
            self.checkAllDataTypeCheckboxes_(true);
          else
            self.restoreDataTypeCheckboxes_();
        }
      };

      this.resetPage_('sync-setup-configure');
      $('sync-setup-configure').hidden = false;

      // onsubmit is changed when submitting a passphrase. Reset it to its
      // default.
      $('choose-data-types-form').onsubmit = function() {
        self.sendConfiguration_();
        return false;
      };

      if (args) {
        this.setCheckboxesAndErrors_(args);

        this.useEncryptEverything_ = args.encryptAllData;

        // Determine whether to display the 'OK, sync everything' confirmation
        // dialog or the advanced sync settings dialog, and assign focus to the
        // OK button, or to the passphrase field if a passphrase is required.
        this.usePassphrase_ = args.usePassphrase;
        if (args.showSyncEverythingPage == false || this.usePassphrase_ ||
            args.syncAllDataTypes == false || args.showPassphrase) {
          var index = args.syncAllDataTypes ?
                          DataTypeSelection.SYNC_EVERYTHING :
                          DataTypeSelection.CHOOSE_WHAT_TO_SYNC;
          this.showCustomizePage_(args, index);
        } else {
          this.showSyncEverythingPage_();
        }
      }
    },

    showSpinner_: function() {
      this.resetPage_('sync-setup-spinner');
      $('sync-setup-spinner').hidden = false;
    },

    showTimeoutPage_: function() {
      this.resetPage_('sync-setup-timeout');
      $('sync-setup-timeout').hidden = false;
    },

    showSyncEverythingPage_: function() {
      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_SyncSetDefault']);

      // Determine whether to bring the OK button into focus.
      var wasConfirmPageHidden = !this.confirmPageVisible_;
      this.confirmPageVisible_ = true;
      this.customizePageVisible_ = false;

      $('confirm-sync-preferences').hidden = false;
      $('customize-sync-preferences').hidden = true;

      // Reset the selection to 'Sync everything'.
      $('sync-select-datatypes').selectedIndex = 0;

      // The default state is to sync everything.
      this.setDataTypeCheckboxes_(DataTypeSelection.SYNC_EVERYTHING);

      if (!this.usePassphrase_)
        $('sync-custom-passphrase').hidden = true;

      if (!this.useEncryptEverything_ && !this.usePassphrase_)
        $('basic-encryption-option').checked = true;

      // Give the OK button focus only when the dialog wasn't already visible.
      if (wasConfirmPageHidden)
        $('confirm-everything-ok').focus();
    },

    /**
     * Reveals the UI for when the user chooses not to sync any data types.
     * This happens when the user signs in and selects "Sync nothing" in the
     * advanced sync settings dialog.
     * @private
     */
    showSyncNothingPage_: function() {
      // Reset the selection to 'Sync nothing'.
      $('sync-select-datatypes').selectedIndex = DataTypeSelection.SYNC_NOTHING;

      // Uncheck and disable the individual data type checkboxes.
      this.checkAllDataTypeCheckboxes_(false);
      this.setDataTypeCheckboxesEnabled_(false);

      // Hide the encryption section.
      $('customize-sync-encryption-new').hidden = true;
      $('sync-custom-passphrase-container').hidden = true;
      $('sync-existing-passphrase-container').hidden = true;

      // Hide the "use default settings" link.
      $('use-default-link').hidden = true;
      $('use-default-link').disabled = true;
      $('use-default-link').onclick = null;
    },

    /**
     * Reveals the UI for entering a custom passphrase during initial setup.
     * This happens if the user has previously enabled a custom passphrase on a
     * different machine.
     * @param {Array} args The args that contain the passphrase UI
     *     configuration.
     * @private
     */
    showPassphraseContainer_: function(args) {
      // Once we require a passphrase, we prevent the user from returning to
      // the Sync Everything pane.
      $('use-default-link').hidden = true;
      $('use-default-link').disabled = true;
      $('use-default-link').onclick = null;
      $('sync-custom-passphrase-container').hidden = true;
      $('sync-existing-passphrase-container').hidden = false;

      // Hide the selection options within the new encryption section when
      // prompting for a passphrase.
      $('sync-new-encryption-section-container').hidden = true;

      $('normal-body').hidden = true;
      $('google-passphrase-needed-body').hidden = true;
      // Display the correct prompt to the user depending on what type of
      // passphrase is needed.
      if (args.usePassphrase)
        $('normal-body').hidden = false;
      else
        $('google-passphrase-needed-body').hidden = false;

      $('passphrase-learn-more').hidden = false;
      // Warn the user about their incorrect passphrase if we need a passphrase
      // and the passphrase field is non-empty (meaning they tried to set it
      // previously but failed).
      $('incorrect-passphrase').hidden =
          !(args.usePassphrase && args.passphraseFailed);

      $('sync-passphrase-warning').hidden = false;
    },

    /**
     * Displays the advanced sync setting dialog, and pre-selects either the
     * "Sync everything" or the "Choose what to sync" drop-down menu item.
     * @param {cr.DataTypeSelection} index Index of item to pre-select.
     * @private
     */
    showCustomizePage_: function(args, index) {
      // Determine whether to bring the OK button field into focus.
      var wasCustomizePageHidden = !this.customizePageVisible_;
      this.customizePageVisible_ = true;
      this.confirmPageVisible_ = false;

      $('confirm-sync-preferences').hidden = true;
      $('customize-sync-preferences').hidden = false;

      $('sync-custom-passphrase-container').hidden = false;
      $('sync-new-encryption-section-container').hidden = false;
      $('customize-sync-encryption-new').hidden = false;

      $('sync-existing-passphrase-container').hidden = true;

      $('sync-select-datatypes').selectedIndex = index;
      this.setDataTypeCheckboxesEnabled_(
          index == DataTypeSelection.CHOOSE_WHAT_TO_SYNC);

      // Give the OK button focus only when the dialog wasn't already visible.
      if (wasCustomizePageHidden)
        $('choose-datatypes-ok').focus();

      if (args && args.showPassphrase) {
        this.showPassphraseContainer_(args);
        // Give the passphrase field focus only when the dialog wasn't already
        // visible.
        if (wasCustomizePageHidden)
          $('passphrase').focus();
      } else {
        // We only show the 'Use Default' link if we're not prompting for an
        // existing passphrase.
        $('use-default-link').hidden = false;
        $('use-default-link').disabled = false;
        $('use-default-link').onclick = function() {
          SyncSetupOverlay.showSyncEverythingPage();
          return false;
        };
      }
    },

    /**
     * Shows the appropriate sync setup page.
     * @param {string} page A page of the sync setup to show.
     * @param {object} args Data from the C++ to forward on to the right
     *     section.
     */
    showSyncSetupPage_: function(page, args) {
      // If the user clicks the OK button, dismiss the dialog immediately, and
      // do not go through the process of hiding elements of the overlay.
      // See crbug.com/308873.
      if (page == 'done') {
        this.closeOverlay_();
        return;
      }

      this.setThrobbersVisible_(false);

      // Hide an existing visible overlay (ensuring the close button is not
      // hidden).
      var children = document.querySelectorAll(
          '#sync-setup-overlay > *:not(.close-button)');
      for (var i = 0; i < children.length; i++)
        children[i].hidden = true;

      this.setInputElementsDisabledState_(false);

      // If new passphrase bodies are present, overwrite the existing ones.
      if (args && args.enterPassphraseBody != undefined)
        $('normal-body').innerHTML = args.enterPassphraseBody;
      if (args && args.enterGooglePassphraseBody != undefined) {
        $('google-passphrase-needed-body').innerHTML =
            args.enterGooglePassphraseBody;
      }
      if (args && args.fullEncryptionBody != undefined)
        $('full-encryption-body').innerHTML = args.fullEncryptionBody;

      // NOTE: Because both showGaiaLogin_() and showConfigure_() change the
      // focus, we need to ensure that the overlay container and dialog aren't
      // [hidden] (as trying to focus() nodes inside of a [hidden] DOM section
      // doesn't work).
      this.showOverlay_();

      if (page == 'configure' || page == 'passphrase')
        this.showConfigure_(args);
      else if (page == 'spinner')
        this.showSpinner_();
      else if (page == 'timeout')
        this.showTimeoutPage_();
    },

    /**
     * Changes the visibility of throbbers on this page.
     * @param {boolean} visible Whether or not to set all throbber nodes
     *     visible.
     */
    setThrobbersVisible_: function(visible) {
      var throbbers = this.pageDiv.getElementsByClassName('throbber');
      for (var i = 0; i < throbbers.length; i++)
        throbbers[i].style.visibility = visible ? 'visible' : 'hidden';
    },

    /**
     * Reset the state of all descendant elements of a root element to their
     * initial state.
     * The initial state is specified by adding a class to the descendant
     * element in sync_setup_overlay.html.
     * @param {HTMLElement} pageElementId The root page element id.
     * @private
     */
    resetPage_: function(pageElementId) {
      var page = $(pageElementId);
      var forEach = function(arr, fn) {
        var length = arr.length;
        for (var i = 0; i < length; i++) {
          fn(arr[i]);
        }
      };

      forEach(page.getElementsByClassName('reset-hidden'),
          function(elt) { elt.hidden = true; });
      forEach(page.getElementsByClassName('reset-shown'),
          function(elt) { elt.hidden = false; });
      forEach(page.getElementsByClassName('reset-disabled'),
          function(elt) { elt.disabled = true; });
      forEach(page.getElementsByClassName('reset-enabled'),
          function(elt) { elt.disabled = false; });
      forEach(page.getElementsByClassName('reset-value'),
          function(elt) { elt.value = ''; });
      forEach(page.getElementsByClassName('reset-opaque'),
          function(elt) { elt.classList.remove('transparent'); });
    },

    /**
     * Displays the stop syncing dialog.
     * @private
     */
    showStopSyncingUI_: function() {
      // Hide any visible children of the overlay.
      var overlay = $('sync-setup-overlay');
      for (var i = 0; i < overlay.children.length; i++)
        overlay.children[i].hidden = true;

      // Bypass OptionsPage.navigateToPage because it will call didShowPage
      // which will set its own visible page, based on the flow state.
      this.visible = true;

      $('sync-setup-stop-syncing').hidden = false;
      $('stop-syncing-cancel').focus();
    },

    /**
     * Determines the appropriate page to show in the Sync Setup UI based on
     * the state of the Sync backend. Does nothing if the user is not signed in.
     * @private
     */
    showSetupUI_: function() {
      chrome.send('SyncSetupShowSetupUI');
      chrome.send('coreOptionsUserMetricsAction', ['Options_SyncShowAdvanced']);
    },

    /**
     * Starts the signin process for the user. Does nothing if the user is
     * already signed in.
     * @private
     */
    startSignIn_: function() {
      chrome.send('SyncSetupStartSignIn');
    },

    /**
     * Forces user to sign out of Chrome for Chrome OS.
     * @private
     */
    doSignOutOnAuthError_: function() {
      chrome.send('SyncSetupDoSignOutOnAuthError');
    },
  };

  // These methods are for general consumption.
  SyncSetupOverlay.closeOverlay = function() {
    SyncSetupOverlay.getInstance().closeOverlay_();
  };

  SyncSetupOverlay.showSetupUI = function() {
    SyncSetupOverlay.getInstance().showSetupUI_();
  };

  SyncSetupOverlay.startSignIn = function() {
    SyncSetupOverlay.getInstance().startSignIn_();
  };

  SyncSetupOverlay.doSignOutOnAuthError = function() {
    SyncSetupOverlay.getInstance().doSignOutOnAuthError_();
  };

  SyncSetupOverlay.showSyncSetupPage = function(page, args) {
    SyncSetupOverlay.getInstance().showSyncSetupPage_(page, args);
  };

  SyncSetupOverlay.showCustomizePage = function(args, index) {
    SyncSetupOverlay.getInstance().showCustomizePage_(args, index);
  };

  SyncSetupOverlay.showSyncEverythingPage = function() {
    SyncSetupOverlay.getInstance().showSyncEverythingPage_();
  };

  SyncSetupOverlay.showStopSyncingUI = function() {
    SyncSetupOverlay.getInstance().showStopSyncingUI_();
  };

  // Export
  return {
    SyncSetupOverlay: SyncSetupOverlay
  };
});
