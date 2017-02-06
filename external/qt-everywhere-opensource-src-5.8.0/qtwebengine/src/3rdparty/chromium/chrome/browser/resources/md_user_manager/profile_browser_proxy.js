// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper object and related behavior that encapsulate messaging
 * between JS and C++ for creating/importing profiles in the user-manager page.
 */

/** @typedef {{username: string, profilePath: string}} */
var SignedInUser;

/**
 * @typedef {{name: string,
 *            filePath: string,
 *            isSupervised: boolean,
 *            custodianUsername: string,
 *            showConfirmation: boolean}}
 */
var ProfileInfo;

/**
 * @typedef {{id: string,
 *            name: string,
 *            iconURL: string,
 *            onCurrentDevice: boolean}}
 */
var SupervisedUser;

cr.define('signin', function() {
  /** @interface */
  function ProfileBrowserProxy() {}

  ProfileBrowserProxy.prototype = {
    /**
     * Gets the available profile icons to choose from.
     */
    getAvailableIcons: function() {
      assertNotReached();
    },

    /**
     * Gets the current signed-in users.
     */
    getSignedInUsers: function() {
      assertNotReached();
    },

    /**
     * Launches the guest user.
     */
    launchGuestUser: function() {
      assertNotReached();
    },

    /**
     * @param {string} profilePath Profile Path of the custodian.
     * @return {!Promise<!Array<!SupervisedUser>>} The list of existing
     *     supervised users.
     */
    getExistingSupervisedUsers: function(profilePath) {
      assertNotReached();
    },

    /**
     * Creates a profile.
     * @param {string} profileName Name of the new profile.
     * @param {string} profileIconUrl URL of the selected icon of the new
     *     profile.
     * @param {boolean} isSupervised True if the new profile is supervised.
     * @param {string} supervisedUserId ID of the supervised user to be
     *     imported.
     * @param {string} custodianProfilePath Profile path of the custodian if
     *     the new profile is supervised.
     */
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisedUserId, custodianProfilePath) {
      assertNotReached();
    },

    /**
     * Cancels creation of the new profile.
     */
    cancelCreateProfile: function() {
      assertNotReached();
    },

    /**
     * Cancels loading supervised users.
     */
    cancelLoadingSupervisedUsers: function() {
      assertNotReached();
    },

    /**
     * Initializes the UserManager
     * @param {string} locationHash
     */
    initializeUserManager: function(locationHash) {
      assertNotReached();
    },

    /**
     * Launches the user with the given |profilePath|
     * @param {string} profilePath Profile Path of the user.
     */
    launchUser: function(profilePath) {
      assertNotReached();
    },

    /**
     * Opens the given url in a new tab in the browser instance of the last
     * active profile. Hyperlinks don't work in the user manager since its
     * browser instance does not support tabs.
     * @param {string} url
     */
    openUrlInLastActiveProfileBrowser: function(url) {
      assertNotReached();
    },

    /**
     * Switches to the profile with the given path.
     * @param {string} profilePath Path to the profile to switch to.
     */
    switchToProfile: function(profilePath) {
      assertNotReached();
    },

    /**
     * @return {!Promise<boolean>} Whether all (non-supervised and non-child)
     *     profiles are locked.
     */
    areAllProfilesLocked: function() {
      assertNotReached();
    },

    /**
     * Authenticates the custodian profile with the given email address.
     * @param {string} emailAddress Email address of the custodian profile.
     */
    authenticateCustodian: function(emailAddress) {
      assertNotReached();
    }
  };

  /**
   * @constructor
   * @implements {signin.ProfileBrowserProxy}
   */
  function ProfileBrowserProxyImpl() {}

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ProfileBrowserProxyImpl);

  ProfileBrowserProxyImpl.prototype = {
    /** @override */
    getAvailableIcons: function() {
      chrome.send('requestDefaultProfileIcons');
    },

    /** @override */
    getSignedInUsers: function() {
      chrome.send('requestSignedInProfiles');
    },

    /** @override */
    launchGuestUser: function() {
      chrome.send('launchGuest');
    },

    /** @override */
    getExistingSupervisedUsers: function(profilePath) {
      return cr.sendWithPromise('getExistingSupervisedUsers', profilePath);
    },

    /** @override */
    createProfile: function(profileName, profileIconUrl, isSupervised,
        supervisedUserId, custodianProfilePath) {
      chrome.send('createProfile',
                  [profileName, profileIconUrl, false, isSupervised,
                   supervisedUserId, custodianProfilePath]);
    },

    /** @override */
    cancelCreateProfile: function() {
      chrome.send('cancelCreateProfile');
    },

    /** @override */
    cancelLoadingSupervisedUsers: function() {
      chrome.send('cancelLoadingSupervisedUsers');
    },

    /** @override */
    initializeUserManager: function(locationHash) {
      chrome.send('userManagerInitialize', [locationHash]);
    },

    /** @override */
    launchUser: function(profilePath) {
      chrome.send('launchUser', [profilePath]);
    },

    /** @override */
    openUrlInLastActiveProfileBrowser: function(url) {
      chrome.send('openUrlInLastActiveProfileBrowser', [url]);
    },

    /** @override */
    switchToProfile: function(profilePath) {
      chrome.send('switchToProfile', [profilePath]);
    },

    /** @override */
    areAllProfilesLocked: function() {
      return cr.sendWithPromise('areAllProfilesLocked');
    },

    /** @override */
    authenticateCustodian: function(emailAddress) {
      chrome.send('authenticateCustodian', [emailAddress]);
    }
  };

  return {
    ProfileBrowserProxy: ProfileBrowserProxy,
    ProfileBrowserProxyImpl: ProfileBrowserProxyImpl,
  };
});
