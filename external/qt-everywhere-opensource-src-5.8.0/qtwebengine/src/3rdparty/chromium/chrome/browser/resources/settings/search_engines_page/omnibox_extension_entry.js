// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-omnibox-extension-entry' is a component for showing
 * an omnibox extension with its name and keyword.
 */
Polymer({
  is: 'settings-omnibox-extension-entry',

  properties: {
    /** @type {!SearchEngine} */
    engine: Object,

    /** @private {!settings.SearchEnginesBrowserProxy} */
    browserProxy_: Object,
  },

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @private */
  onManageTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.manageExtension(this.engine.extension.id);
  },

  /** @private */
  onDisableTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.disableExtension(this.engine.extension.id);
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('iron-dropdown').close();
  },

  /**
   * @param {string} url
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    return cr.icon.getFaviconImageSet(url);
  },
});
