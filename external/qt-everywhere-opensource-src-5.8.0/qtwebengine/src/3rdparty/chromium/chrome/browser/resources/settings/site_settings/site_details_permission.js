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

    i18n_: {
      readOnly: true,
      type: Object,
      value: function() {
        return {
          allowAction: loadTimeData.getString('siteSettingsActionAllow'),
          blockAction: loadTimeData.getString('siteSettingsActionBlock'),
        };
      },
    },
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
    return this.removePatternWildcard_(left) ==
        this.removePatternWildcard_(right);
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
        if (this.sameOrigin_(exceptionList[i].origin, site.origin)) {
          this.$.permission.selected = exceptionList[i].setting;
          this.$.details.hidden = false;
        }
      }
    }.bind(this));
  },

  /**
   * Called when a site within a category has been changed.
   * @param {number} category The category that changed.
   * @param {string} site The site that changed.
   * @private
   */
  sitePermissionChanged_: function(category, site) {
    if (category == this.category && (site == '' || site == this.site.origin)) {
      // TODO(finnur): Send down the full SiteException, not just a string.
      this.siteChanged_({
        origin: site,
        originForDisplay: '',
        embeddingOrigin: '',
        setting: '',
        source: '',
      });
    }
  },

  /**
   * Resets the category permission for this origin.
   */
  resetPermission: function() {
    this.resetCategoryPermissionForOrigin(this.site.origin, '', this.category);
    this.$.details.hidden = true;
  },

  /**
   * Handles the category permission changing for this origin.
   * @param {!{detail: !{item: !{dataset: !{permissionValue: string}}}}} event
   */
  onPermissionMenuIronActivate_: function(event) {
    var value = event.detail.item.dataset.permissionValue;
    this.setCategoryPermissionForOrigin(
        this.site.origin, '', this.category, value);
  },
});
