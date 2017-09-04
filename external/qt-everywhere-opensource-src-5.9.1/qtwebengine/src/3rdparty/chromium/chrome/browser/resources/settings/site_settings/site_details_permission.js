// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details-permission' handles showing the state of one permission, such
 * as Geolocation, for a given origin.
 */
Polymer({
  is: 'site-details-permission',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The site that this widget is showing details for.
     * @type {SiteException}
     */
    site: Object,
  },

  observers: ['siteChanged_(site, category)'],

  /** @override */
  attached: function() {
    this.addWebUIListener('contentSettingSitePermissionChanged',
        this.sitePermissionChanged_.bind(this));
  },

  /**
   * Returns true if the origins match, e.g. http://google.com and
   * http://[*.]google.com.
   * @param {string} left The first origin to compare.
   * @param {string} right The second origin to compare.
   * @return {boolean} True if the origins are the same.
   * @private
   */
  sameOrigin_: function(left, right) {
    return this.removePatternWildcard(left) ==
        this.removePatternWildcard(right);
  },

  /** @private */
  isCookiesCategory_: function(category) {
    return category == settings.ContentSettingsTypes.COOKIES;
  },

  /**
   * Sets the site to display.
   * @param {!SiteException} site The site to display.
   * @private
   */
  siteChanged_: function(site) {
    this.$.details.hidden = true;

    this.browserProxy.getExceptionList(this.category).then(
        function(exceptionList) {
      for (var i = 0; i < exceptionList.length; ++i) {
        if (exceptionList[i].embeddingOrigin == site.embeddingOrigin &&
            this.sameOrigin_(exceptionList[i].origin, site.origin)) {
          this.$.permission.value = exceptionList[i].setting;
          this.$.details.hidden = false;
          break;
        }
      }
    }.bind(this));
  },

  /**
   * Called when a site within a category has been changed.
   * @param {number} category The category that changed.
   * @param {string} origin The origin of the site that changed.
   * @param {string} embeddingOrigin The embedding origin of the site that
   *     changed.
   * @private
   */
  sitePermissionChanged_: function(category, origin, embeddingOrigin) {
    if (this.site === undefined)
      return;
    if (category != this.category)
      return;

    if (origin == '' || (origin == this.site.origin &&
                         embeddingOrigin == this.site.embeddingOrigin)) {
      this.siteChanged_(this.site);
    }
  },

  /**
   * Resets the category permission for this origin.
   */
  resetPermission: function() {
    this.browserProxy.resetCategoryPermissionForOrigin(
        this.site.origin, this.site.embeddingOrigin, this.category,
        this.site.incognito);
    this.$.details.hidden = true;
  },

  /**
   * Handles the category permission changing for this origin.
   * @private
   */
  onPermissionSelectionChange_: function() {
    this.browserProxy.setCategoryPermissionForOrigin(
        this.site.origin, this.site.embeddingOrigin, this.category,
        this.$.permission.value, this.site.incognito);
  },
});
