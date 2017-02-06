// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'settings-people-page',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * The currently selected profile icon URL. May be a data URL.
     */
    profileIconUrl_: String,

    /**
     * The current profile name.
     */
    profileName_: String,

    /**
     * True if the current profile manages supervised users.
     */
    profileManagesSupervisedUsers_: Boolean,

    /** @private {!settings.SyncBrowserProxy} */
    syncBrowserProxy_: {
      type: Object,
      value: function() {
        return settings.SyncBrowserProxyImpl.getInstance();
      },
    },

<if expr="chromeos">
    /**
     * True if quick unlock settings should be displayed on this machine.
     * @private
     */
    quickUnlockAllowed_: {
      type: Boolean,
      // TODO(jdufault): Get a real value via quickUnlockPrivate API.
      value: false,
      readOnly: true,
    },

    /** @private {!settings.EasyUnlockBrowserProxy} */
    easyUnlockBrowserProxy_: {
      type: Object,
      value: function() {
        return settings.EasyUnlockBrowserProxyImpl.getInstance();
      },
    },

    /**
     * True if Easy Unlock is allowed on this machine.
     */
    easyUnlockAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed');
      },
      readOnly: true,
    },

    /**
     * True if Easy Unlock is enabled.
     */
    easyUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockEnabled');
      },
    },

    /**
     * True if Easy Unlock's proximity detection feature is allowed.
     */
    easyUnlockProximityDetectionAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed') &&
            loadTimeData.getBoolean('easyUnlockProximityDetectionAllowed');
      },
      readOnly: true,
    },
</if>
  },

  /** @override */
  attached: function() {
    var profileInfoProxy = settings.ProfileInfoBrowserProxyImpl.getInstance();
    profileInfoProxy.getProfileInfo().then(this.handleProfileInfo_.bind(this));
    this.addWebUIListener('profile-info-changed',
                          this.handleProfileInfo_.bind(this));

    profileInfoProxy.getProfileManagesSupervisedUsers().then(
        this.handleProfileManagesSupervisedUsers_.bind(this));
    this.addWebUIListener('profile-manages-supervised-users-changed',
                          this.handleProfileManagesSupervisedUsers_.bind(this));

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener('sync-status-changed',
                          this.handleSyncStatus_.bind(this));

<if expr="chromeos">
    if (this.easyUnlockAllowed_) {
      this.addWebUIListener(
          'easy-unlock-enabled-status',
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
      this.easyUnlockBrowserProxy_.getEnabledStatus().then(
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
    }
</if>
  },

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_: function(info) {
    this.profileName_ = info.name;
    this.profileIconUrl_ = info.iconUrl;
  },

  /**
   * Handler for when the profile starts or stops managing supervised users.
   * @private
   * @param {boolean} managesSupervisedUsers
   */
  handleProfileManagesSupervisedUsers_: function(managesSupervisedUsers) {
    this.profileManagesSupervisedUsers_ = managesSupervisedUsers;
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

<if expr="chromeos">
  /**
   * Handler for when the Easy Unlock enabled status has changed.
   * @private
   */
  handleEasyUnlockEnabledStatusChanged_: function(easyUnlockEnabled) {
    this.easyUnlockEnabled_ = easyUnlockEnabled;
  },
</if>

  /** @private */
  onPictureTap_: function() {
<if expr="chromeos">
    this.$.pages.setSubpageChain(['changePicture']);
</if>
<if expr="not chromeos">
    this.$.pages.setSubpageChain(['manageProfile']);
</if>
  },

<if expr="not chromeos">
  /** @private */
  onProfileNameTap_: function() {
    this.$.pages.setSubpageChain(['manageProfile']);
  },
</if>

  /** @private */
  onActivityControlsTap_: function() {
    this.syncBrowserProxy_.openActivityControlsUrl();
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectTap_: function() {
    this.$.disconnectDialog.open();
  },

  /** @private */
  onDisconnectCancel_: function() {
    this.$.disconnectDialog.close();
  },

  /** @private */
  onDisconnectConfirm_: function() {
    var deleteProfile = !!this.syncStatus.domain ||
        (this.$.deleteProfile && this.$.deleteProfile.checked);
    this.syncBrowserProxy_.signOut(deleteProfile);

    this.$.disconnectDialog.close();
  },

  /** @private */
  onSyncTap_: function() {
    assert(this.syncStatus.signedIn);
    assert(this.syncStatus.syncSystemEnabled);

    if (this.syncStatus.managed)
      return;

    this.$.pages.setSubpageChain(['sync']);
  },

<if expr="chromeos">
  /** @private */
  onQuickUnlockTap_: function() {
    this.$.pages.setSubpageChain(['quick-unlock-authenticate']);
  },

  /** @private */
  onEasyUnlockSetupTap_: function() {
    this.easyUnlockBrowserProxy_.startTurnOnFlow();
  },

  /** @private */
  onEasyUnlockTurnOffTap_: function() {
    this.$$('#easyUnlockTurnOffDialog').open();
  },
</if>

  /** @private */
  onManageOtherPeople_: function() {
<if expr="not chromeos">
    this.syncBrowserProxy_.manageOtherPeople();
</if>
<if expr="chromeos">
    this.$.pages.setSubpageChain(['users']);
</if>
  },

  /** @private */
  onManageSupervisedUsers_: function() {
    window.open(loadTimeData.getString('supervisedUsersUrl'));
  },

<if expr="not chromeos">
  /**
   * @private
   * @param {string} domain
   * @return {string}
   */
  getDomainHtml_: function(domain) {
    var innerSpan =
        '<span id="managed-by-domain-name">' + domain + '</span>';
    return loadTimeData.getStringF('domainManagedProfile', innerSpan);
  },
</if>

  /**
   * @private
   * @param {string} domain
   * @return {string}
   */
  getDisconnectExplanationHtml_: function(domain) {
<if expr="not chromeos">
    if (domain) {
      return loadTimeData.getStringF(
          'syncDisconnectManagedProfileExplanation',
          '<span id="managed-by-domain-name">' + domain + '</span>');
    }
</if>
    return loadTimeData.getString('syncDisconnectExplanation');
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean}
   */
  isAdvancedSyncSettingsVisible_: function(syncStatus) {
    return !!syncStatus && !!syncStatus.signedIn &&
        !!syncStatus.syncSystemEnabled;
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {string}
   */
  getSyncIcon_: function(syncStatus) {
    if (!syncStatus)
      return '';
    if (syncStatus.hasError)
      return 'settings:sync-problem';
    if (syncStatus.managed)
      return 'settings:sync-disabled';

    return 'settings:done';
  },
});
