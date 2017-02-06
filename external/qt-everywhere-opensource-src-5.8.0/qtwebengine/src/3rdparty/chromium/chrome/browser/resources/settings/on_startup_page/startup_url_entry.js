// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displayes a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

cr.exportPath('settings');

/**
 * The name of the event fired from this element when the "Edit" option is
 * tapped.
 * @const {string}
 */
settings.EDIT_STARTUP_URL_EVENT = 'edit-startup-url';

Polymer({
  is: 'settings-startup-url-entry',

  properties: {
    /** @type {!StartupPageInfo} */
    model: Object,
  },

  /**
   * @param {string} url Location of an image to get a set of icons for.
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    return cr.icon.getFaviconImageSet(url);
  },

  /** @private */
  onRemoveTap_: function() {
    this.$$('iron-dropdown').close();
    settings.StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  },

  /** @private */
  onEditTap_: function() {
    this.$$('iron-dropdown').close();
    this.fire(settings.EDIT_STARTUP_URL_EVENT, this.model);
  },
});
