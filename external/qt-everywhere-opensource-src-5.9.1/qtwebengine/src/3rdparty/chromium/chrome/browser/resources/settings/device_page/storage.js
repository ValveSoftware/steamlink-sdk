// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-storage' is the settings subpage for storage settings.
 */
cr.exportPath('settings');

/**
 * Enumeration for device state about remaining space.
 * These values must be kept in sync with
 * StorageManagerHandler::StorageSpaceState in C++ code.
 * @enum {number}
 */
settings.StorageSpaceState = {
  NORMAL: 0,
  LOW: 1,
  CRITICALLY_LOW: 2
};

/**
 * @typedef {{
 *   totalSize: string,
 *   availableSize: string,
 *   usedSize: string,
 *   usedRatio: number,
 *   spaceState: settings.StorageSpaceState,
 * }}
 */
settings.StorageSizeStat;

Polymer({
  is: 'settings-storage',

  behaviors: [settings.RouteObserverBehavior, WebUIListenerBehavior],

  properties: {
    /** @private */
    driveEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    androidEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private {settings.StorageSizeStat} */
    sizeStat_: Object,
  },

  /**
   * Timer ID for periodic update.
   * @private {number}
   */
  updateTimerId_: -1,

  /** @override */
  ready: function() {
    cr.addWebUIListener(
        'storage-size-stat-changed',
        this.handleSizeStatChanged_.bind(this));
    cr.addWebUIListener(
        'storage-downloads-size-changed',
        this.handleDownloadsSizeChanged_.bind(this));
    cr.addWebUIListener(
        'storage-drive-cache-size-changed',
        this.handleDriveCacheSizeChanged_.bind(this));
    cr.addWebUIListener(
        'storage-browsing-data-size-changed',
        this.handleBrowsingDataSizeChanged_.bind(this));
    cr.addWebUIListener(
        'storage-android-size-changed',
        this.handleAndroidSizeChanged_.bind(this));
    cr.addWebUIListener(
        'storage-other-users-size-changed',
        this.handleOtherUsersSizeChanged_.bind(this));
    cr.addWebUIListener(
        'storage-drive-enabled-changed',
        this.handleDriveEnabledChanged_.bind(this));
    cr.addWebUIListener(
        'storage-android-enabled-changed',
        this.handleAndroidEnabledChanged_.bind(this));
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @protected
   */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.Route.STORAGE)
      this.onPageShown_();
  },

  /** @private */
  onPageShown_: function() {
    // Updating storage information can be expensive (e.g. computing directory
    // sizes recursively), so we delay this operation until the page is shown.
    chrome.send('updateStorageInfo');
    // We update the storage usage periodically when the overlay is visible.
    this.startPeriodicUpdate_();
  },

  /**
   * Handler for tapping the "Downloads" item.
   * @private
   */
  onDownloadsTap_: function() {
    chrome.send('openDownloads');
  },

  /**
   * Handler for tapping the "Offline files" item.
   * @private
   */
  onDriveCacheTap_: function() {
    this.$.storageDriveCache.open();
  },

  /**
   * Handler for tapping the "Browsing data" item.
   * @private
   */
  onBrowsingDataTap_: function() {
    settings.navigateTo(settings.Route.CLEAR_BROWSER_DATA);
  },

  /**
   * Handler for tapping the "Android storage" item.
   * @private
   */
  onAndroidTap_: function() {
    chrome.send('openArcStorage');
  },

  /**
   * Handler for tapping the "Other users" item.
   * @private
   */
  onOtherUsersTap_: function() {
    settings.navigateTo(settings.Route.ACCOUNTS);
  },

  /**
   * @param {!settings.StorageSizeStat} sizeStat
   * @private
   */
  handleSizeStatChanged_: function(sizeStat) {
    this.sizeStat_ = sizeStat;
    this.$.inUseLabelArea.style.width = (sizeStat.usedRatio * 100) + '%';
    this.$.availableLabelArea.style.width =
        ((1 - sizeStat.usedRatio) * 100) + '%';
  },

  /**
   * @param {string} size Formatted string representing the size of Downloads.
   * @private
   */
  handleDownloadsSizeChanged_: function(size) {
    this.$.downloadsSize.textContent = size;
  },

  /**
   * @param {string} size Formatted string representing the size of Offline
   *     files.
   * @private
   */
  handleDriveCacheSizeChanged_: function(size) {
    this.$.driveCacheSize.textContent = size;
  },

  /**
   * @param {string} size Formatted string representing the size of Browsing
   *     data.
   * @private
   */
  handleBrowsingDataSizeChanged_: function(size) {
    this.$.browsingDataSize.textContent = size;
  },

  /**
   * @param {string} size Formatted string representing the size of Android
   *     storage.
   * @private
   */
  handleAndroidSizeChanged_: function(size) {
    this.$.androidSize.textContent = size;
  },

  /**
   * @param {string} size Formatted string representing the size of Other users.
   * @private
   */
  handleOtherUsersSizeChanged_: function(size) {
    this.$.otherUsersSize.textContent = size;
  },

  /**
   * @param {boolean} enabled True if Google Drive is enabled.
   * @private
   */
  handleDriveEnabledChanged_: function(enabled) {
    this.driveEnabled_ = enabled;
  },

  /**
   * @param {boolean} enabled True if Android Play Store is enabled.
   * @private
   */
  handleAndroidEnabledChanged_: function(enabled) {
    this.androidEnabled_ = enabled;
  },

  /**
   * Starts periodic update for storage usage.
   * @private
   */
  startPeriodicUpdate_: function() {
    // We update the storage usage every 5 seconds.
    if (this.updateTimerId_ == -1) {
      this.updateTimerId_ = window.setInterval(function() {
        if (settings.getCurrentRoute() != settings.Route.STORAGE) {
          this.stopPeriodicUpdate_();
          return;
        }
        chrome.send('updateStorageInfo');
      }.bind(this), 5000);
    }
  },

  /**
   * Stops periodic update for storage usage.
   * @private
   */
  stopPeriodicUpdate_: function() {
    if (this.updateTimerId_ != -1) {
      window.clearInterval(this.updateTimerId_);
      this.updateTimerId_ = -1;
    }
  },

  /**
   * Returns true if the remaining space is low, but not critically low.
   * @param {!settings.StorageSpaceState} spaceState Status about the remaining
   *     space.
   * @private
   */
  isSpaceLow_: function(spaceState) {
    return spaceState == settings.StorageSpaceState.LOW;
  },

  /**
   * Returns true if the remaining space is critically low.
   * @param {!settings.StorageSpaceState} spaceState Status about the remaining
   *     space.
   * @private
   */
  isSpaceCriticallyLow_: function(spaceState) {
    return spaceState == settings.StorageSpaceState.CRITICALLY_LOW;
  },

  /**
   * Computes class name of the bar based on the remaining space size.
   * @param {!settings.StorageSpaceState} spaceState Status about the remaining
   *     space.
   * @private
   */
  getBarClass_: function(spaceState) {
    switch (spaceState) {
      case settings.StorageSpaceState.LOW:
        return 'space-low';
      case settings.StorageSpaceState.CRITICALLY_LOW:
        return 'space-critically-low';
      default:
        return '';
    }
  },
});
