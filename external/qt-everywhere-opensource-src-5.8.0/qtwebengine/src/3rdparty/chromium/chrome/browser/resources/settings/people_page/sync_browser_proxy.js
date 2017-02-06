// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to get the
 * status of the sync backend and user preferences on what data to sync. Used
 * for both Chrome browser and ChromeOS.
 */
cr.exportPath('settings');

/**
 * @typedef {{actionLinkText: (string|undefined),
 *            childUser: (boolean|undefined),
 *            domain: (string|undefined),
 *            hasError: (boolean|undefined),
 *            hasUnrecoverableError: (boolean|undefined),
 *            managed: (boolean|undefined),
 *            setupCompleted: (boolean|undefined),
 *            setupInProgress: (boolean|undefined),
 *            signedIn: (boolean|undefined),
 *            signinAllowed: (boolean|undefined),
 *            statusText: (string|undefined),
 *            supervisedUser: (boolean|undefined),
 *            syncSystemEnabled: (boolean|undefined)}}
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
settings.SyncStatus;

/**
 * The state of sync. This is the data structure sent back and forth between
 * C++ and JS. Its naming and structure is not optimal, but changing it would
 * require changes to the C++ handler, which is already functional.
 * @typedef {{
 *   appsEnforced: boolean,
 *   appsRegistered: boolean,
 *   appsSynced: boolean,
 *   autofillEnforced: boolean,
 *   autofillRegistered: boolean,
 *   autofillSynced: boolean,
 *   bookmarksEnforced: boolean,
 *   bookmarksRegistered: boolean,
 *   bookmarksSynced: boolean,
 *   encryptAllData: boolean,
 *   encryptAllDataAllowed: boolean,
 *   enterGooglePassphraseBody: (string|undefined),
 *   enterPassphraseBody: (string|undefined),
 *   extensionsEnforced: boolean,
 *   extensionsRegistered: boolean,
 *   extensionsSynced: boolean,
 *   fullEncryptionBody: string,
 *   passphrase: (string|undefined),
 *   passphraseRequired: boolean,
 *   passphraseTypeIsCustom: boolean,
 *   passwordsEnforced: boolean,
 *   passwordsRegistered: boolean,
 *   passwordsSynced: boolean,
 *   paymentsIntegrationEnabled: boolean,
 *   preferencesEnforced: boolean,
 *   preferencesRegistered: boolean,
 *   preferencesSynced: boolean,
 *   setNewPassphrase: (boolean|undefined),
 *   syncAllDataTypes: boolean,
 *   tabsEnforced: boolean,
 *   tabsRegistered: boolean,
 *   tabsSynced: boolean,
 *   themesEnforced: boolean,
 *   themesRegistered: boolean,
 *   themesSynced: boolean,
 *   typedUrlsEnforced: boolean,
 *   typedUrlsRegistered: boolean,
 *   typedUrlsSynced: boolean,
 * }}
 */
settings.SyncPrefs;

/**
 * @enum {string}
 */
settings.PageStatus = {
  SPINNER: 'spinner',                     // Before the page has loaded.
  CONFIGURE: 'configure',                 // Preferences ready to be configured.
  TIMEOUT: 'timeout',                     // Preferences loading has timed out.
  DONE: 'done',                           // Sync subpage can be closed now.
  PASSPHRASE_FAILED: 'passphraseFailed',  // Error in the passphrase.
};

cr.define('settings', function() {
  /** @interface */
  function SyncBrowserProxy() {}

  SyncBrowserProxy.prototype = {
<if expr="not chromeos">
    /**
     * Starts the signin process for the user. Does nothing if the user is
     * already signed in.
     */
    startSignIn: function() {},

    /**
     * Signs out the signed-in user.
     * @param {boolean} deleteProfile
     */
    signOut: function(deleteProfile) {},

    /**
     * Opens the multi-profile user manager.
     */
    manageOtherPeople: function() {},
</if>

    /**
     * Gets the current sync status.
     * @return {!Promise<!settings.SyncStatus>}
     */
    getSyncStatus: function() {},

    /**
     * Function to invoke when the sync page has been navigated to. This
     * registers the UI as the "active" sync UI so that if the user tries to
     * open another sync UI, this one will be shown instead.
     */
    didNavigateToSyncPage: function() {},

    /**
     * Function to invoke when leaving the sync page so that the C++ layer can
     * be notified that the sync UI is no longer open.
     */
    didNavigateAwayFromSyncPage: function() {},

    /**
     * Sets which types of data to sync.
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncDatatypes: function(syncPrefs) {},

    /**
     * Sets the sync encryption options.
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncEncryption: function(syncPrefs) {},

    /**
     * Opens the Google Activity Controls url in a new tab.
     */
    openActivityControlsUrl: function() {},
  };

  /**
   * @constructor
   * @implements {settings.SyncBrowserProxy}
   */
  function SyncBrowserProxyImpl() {}
  cr.addSingletonGetter(SyncBrowserProxyImpl);

  SyncBrowserProxyImpl.prototype = {
<if expr="not chromeos">
    /** @override */
    startSignIn: function() {
      // TODO(tommycli): Currently this is always false, but this will become
      // a parameter once supervised users are implemented in MD Settings.
      var creatingSupervisedUser = false;
      chrome.send('SyncSetupStartSignIn', [creatingSupervisedUser]);
    },

    /** @override */
    signOut: function(deleteProfile) {
      chrome.send('SyncSetupStopSyncing', [deleteProfile]);
    },

    /** @override */
    manageOtherPeople: function() {
      chrome.send('SyncSetupManageOtherPeople');
    },
</if>

    /** @override */
    getSyncStatus: function() {
      return cr.sendWithPromise('SyncSetupGetSyncStatus');
    },

    /** @override */
    didNavigateToSyncPage: function() {
      chrome.send('SyncSetupShowSetupUI');
    },

    /** @override */
    didNavigateAwayFromSyncPage: function() {
      chrome.send('SyncSetupDidClosePage');
    },

    /** @override */
    setSyncDatatypes: function(syncPrefs) {
      return cr.sendWithPromise('SyncSetupSetDatatypes',
                                JSON.stringify(syncPrefs));
    },

    /** @override */
    setSyncEncryption: function(syncPrefs) {
      return cr.sendWithPromise('SyncSetupSetEncryption',
                                JSON.stringify(syncPrefs));
    },

    /** @override */
    openActivityControlsUrl: function() {
      chrome.metricsPrivate.recordUserAction(
          'Signin_AccountSettings_GoogleActivityControlsClicked');
      window.open(loadTimeData.getString('activityControlsUrl'));
    }
  };

  return {
    SyncBrowserProxy: SyncBrowserProxy,
    SyncBrowserProxyImpl: SyncBrowserProxyImpl,
  };
});
