// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to get the
 * profile info, which consists of the profile name and icon. Used for both
 * Chrome browser and ChromeOS.
 */
cr.exportPath('settings');

/**
 * An object describing the profile.
 * @typedef {{
 *   name: string,
 *   iconUrl: string
 * }}
 */
settings.ProfileInfo;

cr.define('settings', function() {
  /** @interface */
  function ProfileInfoBrowserProxy() {}

  ProfileInfoBrowserProxy.prototype = {
    /**
     * Returns a Promise for the profile info.
     * @return {!Promise<!settings.ProfileInfo>}
     */
    getProfileInfo: function() {},

    /**
     * Returns a Promise that's true if the profile manages supervised users.
     * @return {!Promise<boolean>}
     */
    getProfileManagesSupervisedUsers() {},
  };

  /**
   * @constructor
   * @implements {ProfileInfoBrowserProxy}
   */
  function ProfileInfoBrowserProxyImpl() {}
  cr.addSingletonGetter(ProfileInfoBrowserProxyImpl);

  ProfileInfoBrowserProxyImpl.prototype = {
    /** @override */
    getProfileInfo: function() {
      return cr.sendWithPromise('getProfileInfo');
    },

    /** @override */
    getProfileManagesSupervisedUsers() {
      return cr.sendWithPromise('getProfileManagesSupervisedUsers');
    },
  };

  return {
    ProfileInfoBrowserProxyImpl: ProfileInfoBrowserProxyImpl,
  };
});
