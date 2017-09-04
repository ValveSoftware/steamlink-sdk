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

  behaviors: [SiteSettingsBehavior, settings.RouteObserverBehavior],

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
     * @private
     */
    storedData_: {
      type: String,
      value: '',
    },

    /**
     * The type of storage for the origin.
     * @private
     */
    storageType_: Number,

    /** @private */
    confirmationDeleteMsg_: String,
  },

  listeners: {
    'usage-deleted': 'onUsageDeleted_',
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    var site = settings.getQueryParameters().get('site');
    if (!site)
      return;
    this.browserProxy.getSiteDetails(site).then(function(siteInfo) {
      this.site = this.expandSiteException(siteInfo);
    }.bind(this));
  },

  /**
   * Handler for when the origin changes.
   * @private
   */
  onSiteChanged_: function() {
    // origin may be initially undefined if the user follows a direct
    // link (URL) to this page.
    var origin = this.site.origin;
    if (origin !== undefined)
      this.$.usageApi.fetchUsageTotal(this.toUrl(origin).hostname);
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Confirms the deletion of storage for a site.
   * @private
   */
  onConfirmClearStorage_: function() {
    this.confirmationDeleteMsg_ = loadTimeData.getStringF(
        'siteSettingsSiteRemoveConfirmation',
        this.toUrl(this.site.origin).href);
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Clears all data stored for the current origin.
   * @private
   */
  onClearStorage_: function() {
    this.$.usageApi.clearUsage(
        this.toUrl(this.site.origin).href, this.storageType_);
  },

  /**
   * Called when usage has been deleted for an origin.
   * @param {!{detail: !{origin: string}}} event
   * @private
   */
  onUsageDeleted_: function(event) {
    if (event.detail.origin == this.toUrl(this.site.origin).href) {
      this.storedData_ = '';
      this.navigateBackIfNoData_();
    }
  },

  /**
   * Resets all permissions and clears all data stored for the current origin.
   * @private
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
   * @private
   */
  navigateBackIfNoData_: function() {
    if (this.storedData_ == '' && !this.permissionShowing_())
      settings.navigateToPreviousRoute();
  },

  /**
   * Returns true if one or more permission is showing.
   * @private
   */
  permissionShowing_: function() {
    return Array.prototype.some.call(
        this.root.querySelectorAll('site-details-permission'),
        function(element) { return element.offsetHeight > 0; });
  },
});
