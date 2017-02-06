// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-details' show the details (permissions and usage) for a given origin
 * under Site Settings.
 */
Polymer({
  is: 'site-details',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * The site that this widget is showing details for.
     * @type {SiteException}
     */
    site: {
      type: Object,
      observer: 'onSiteChanged_',
    },

    /**
     * The amount of data stored for the origin.
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The type of storage for the origin.
     */
    storageType_: Number,
  },

  listeners: {
    'usage-deleted': 'onUsageDeleted',
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /**
   * Handler for when the origin changes.
   */
  onSiteChanged_: function() {
    // Using originForDisplay avoids the [*.] prefix that some exceptions use.
    var url = new URL(this.ensureUrlHasScheme(this.site.originForDisplay));
    this.$.usageApi.fetchUsageTotal(url.hostname);
  },

  /**
   * Clears all data stored for the current origin.
   */
  onClearStorage_: function() {
    this.$.usageApi.clearUsage(this.site.origin, this.storageType_);
  },

  /**
   * Called when usage has been deleted for an origin.
   */
  onUsageDeleted: function(event) {
    if (event.detail.origin == this.site.origin) {
      this.storedData_ = '';
      this.navigateBackIfNoData_();
    }
  },

  /**
   * Resets all permissions and clears all data stored for the current origin.
   */
  onClearAndReset_: function() {
    Array.prototype.forEach.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { element.resetPermission(); });

    if (this.storedData_ != '')
      this.onClearStorage_();
    else
      this.navigateBackIfNoData_();
  },

  /**
   * Navigate back if the UI is empty (everything been cleared).
   */
  navigateBackIfNoData_: function() {
    if (this.storedData_ == '' && !this.permissionShowing_())
      this.fire('subpage-back');
  },

  /**
   * Returns true if one or more permission is showing.
   */
  permissionShowing_: function() {
    return Array.prototype.some.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { return element.offsetHeight > 0; });
  },
});
