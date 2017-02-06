// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{device: string,
 *           lastUpdateTime: string,
 *           separatorIndexes: !Array<number>,
 *           timestamp: number,
 *           tabs: !Array<!ForeignSessionTab>,
 *           tag: string}}
 */
var ForeignDeviceInternal;

Polymer({
  is: 'history-synced-device-manager',

  properties: {
    /**
     * @type {?Array<!ForeignSession>}
     */
    sessionList: {
      type: Array,
      observer: 'updateSyncedDevices'
    },

    searchedTerm: {
      type: String,
      observer: 'searchTermChanged'
    },

    /**
     * An array of synced devices with synced tab data.
     * @type {!Array<!ForeignDeviceInternal>}
     */
    syncedDevices_: {
      type: Array,
      value: function() { return []; }
    },

    /** @private */
    signInState_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isUserSignedIn'),
    },

    /** @private */
    fetchingSyncedTabs_: {
      type: Boolean,
      value: false,
    }
  },

  /**
   * @param {!ForeignSession} session
   * @return {!ForeignDeviceInternal}
   */
  createInternalDevice_: function(session) {
    var tabs = [];
    var separatorIndexes = [];
    for (var i = 0; i < session.windows.length; i++) {
      var newTabs = session.windows[i].tabs;
      if (newTabs.length == 0)
        continue;


      if (!this.searchedTerm) {
        // Add all the tabs if there is no search term.
        tabs = tabs.concat(newTabs);
        separatorIndexes.push(tabs.length - 1);
      } else {
        var searchText = this.searchedTerm.toLowerCase();
        var windowAdded = false;
        for (var j = 0; j < newTabs.length; j++) {
          var tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) != -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
        if (windowAdded)
          separatorIndexes.push(tabs.length - 1);
      }

    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag,
    };
  },


  onSignInTap_: function() {
    chrome.send('SyncSetupShowSetupUI');
    chrome.send('SyncSetupStartSignIn', [false]);
  },

  /** @private */
  clearDisplayedSyncedDevices_: function() {
    this.syncedDevices_ = [];
  },

  /**
   * Decide whether or not should display no synced tabs message.
   * @param {boolean} signInState
   * @param {number} syncedDevicesLength
   * @return {boolean}
   */
  showNoSyncedMessage: function(signInState, syncedDevicesLength) {
    return signInState && syncedDevicesLength == 0;
  },

  /**
   * Decide what message should be displayed when user is logged in and there
   * are no synced tabs.
   * @param {boolean} fetchingSyncedTabs
   * @return {string}
   */
  noSyncedTabsMessage: function(fetchingSyncedTabs) {
    return loadTimeData.getString(
        fetchingSyncedTabs ? 'loading' : 'noSyncedResults');
  },

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   * @param {?Array<!ForeignSession>} sessionList
   */
  updateSyncedDevices: function(sessionList) {
    this.fetchingSyncedTabs_ = false;

    if (!sessionList)
      return;

    // First, update any existing devices that have changed.
    var updateCount = Math.min(sessionList.length, this.syncedDevices_.length);
    for (var i = 0; i < updateCount; i++) {
      var oldDevice = this.syncedDevices_[i];
      if (oldDevice.tag != sessionList[i].tag ||
          oldDevice.timestamp != sessionList[i].timestamp) {
        this.splice(
            'syncedDevices_', i, 1, this.createInternalDevice_(sessionList[i]));
      }
    }

    // Then, append any new devices.
    for (var i = updateCount; i < sessionList.length; i++) {
      this.push('syncedDevices_', this.createInternalDevice_(sessionList[i]));
    }
  },

  /**
   * Get called when user's sign in state changes, this will affect UI of synced
   * tabs page. Sign in promo gets displayed when user is signed out, and
   * different messages are shown when there are no synced tabs.
   * @param {boolean} isUserSignedIn
   */
  updateSignInState: function(isUserSignedIn) {
    // If user's sign in state didn't change, then don't change message or
    // update UI.
    if (this.signInState_ == isUserSignedIn)
      return;

    this.signInState_ = isUserSignedIn;

    // User signed out, clear synced device list and show the sign in promo.
    if (!isUserSignedIn) {
      this.clearDisplayedSyncedDevices_();
      return;
    }
    // User signed in, show the loading message when querying for synced
    // devices.
    this.fetchingSyncedTabs_ = true;
  },

  searchTermChanged: function(searchedTerm) {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices(this.sessionList);
  }
});
