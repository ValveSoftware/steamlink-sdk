// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage Profile" subpage of
 * the People section to interact with the browser. Chrome Browser only.
 */
cr.define('settings', function() {
  /** @interface */
  function ManageProfileBrowserProxy() {}

  ManageProfileBrowserProxy.prototype = {
    /**
     * Gets the available profile icons to choose from.
     * @return {!Promise<!Array<string>>}
     */
    getAvailableIcons: function() {},

    /**
     * Sets the profile's icon and name. There is no response.
     * @param {!string} iconUrl The new profile URL.
     * @param {!string} name The new profile name.
     */
    setProfileIconAndName: function(iconUrl, name) {},
  };

  /**
   * @constructor
   * @implements {settings.ManageProfileBrowserProxy}
   */
  function ManageProfileBrowserProxyImpl() {}
  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(ManageProfileBrowserProxyImpl);

  ManageProfileBrowserProxyImpl.prototype = {
    /** @override */
    getAvailableIcons: function() {
      return cr.sendWithPromise('getAvailableIcons');
    },

    /** @override */
    setProfileIconAndName: function(iconUrl, name) {
      chrome.send('setProfileIconAndName', [iconUrl, name]);
    },
  };

  return {
    ManageProfileBrowserProxy: ManageProfileBrowserProxy,
    ManageProfileBrowserProxyImpl: ManageProfileBrowserProxyImpl,
  };
});
