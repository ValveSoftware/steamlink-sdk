// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

/**
 * 'site-data-details-subpage' Display cookie contents.
 */
Polymer({
  is: 'site-data-details-subpage',

  behaviors: [settings.RouteObserverBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The browser proxy used to retrieve and change cookies.
     * @type {settings.SiteSettingsPrefsBrowserProxy}
     */
    browserProxy: Object,

    /**
     * The cookie entries for the given site.
     * @type {!Array<!CookieDataItem>}
     * @private
     */
    entries_: Array,

    /** Set the page title on the settings-subpage parent. */
    pageTitle: {
      type: String,
      notify: true,
    },

    /** @private */
    site_: String,

    /** @private */
    siteId_: String,
  },

  /** @override */
  ready: function() {
    this.browserProxy =
        settings.SiteSettingsPrefsBrowserProxyImpl.getInstance();

    this.addWebUIListener('onTreeItemRemoved',
                          this.getCookieDetails_.bind(this));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (settings.getCurrentRoute() != settings.Route.SITE_SETTINGS_DATA_DETAILS)
      return;
    var site = settings.getQueryParameters().get('site');
    if (!site || site == this.site_)
      return;
    this.site_ = site;
    this.pageTitle = loadTimeData.getStringF('siteSettingsCookieSubpage', site);
    this.getCookieDetails_();
  },

  /** @private */
  getCookieDetails_: function() {
    if (!this.site_)
      return;
    this.browserProxy.getCookieDetails(this.site_).then(
        this.onCookiesLoaded_.bind(this),
        this.onCookiesLoadFailed_.bind(this));
  },

  /**
   * @return {!Array<!CookieDataForDisplay>}
   * @private
   */
  getCookieNodes_: function(node) {
    return getCookieData(node);
  },

  /**
   * @param {!CookieDataSummaryItem} cookies
   * @private
   */
  onCookiesLoaded_: function(cookies) {
    this.siteId_ = cookies.id;
    this.entries_ = cookies.children;
    // Set up flag for expanding cookie details.
    this.entries_.map(function(e) { return e.expanded_ = false; });
  },

  /**
   * The site was not found. E.g. The site data may have been deleted or the
   * site URL parameter may be mistyped.
   * @private
   */
  onCookiesLoadFailed_: function() {
    this.siteId_ = '';
    this.entries_ = [];
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!CookieDetails} item
   * @return {string}
   * @private
   */
  getEntryDescription_: function(item) {
    // Frequently there are multiple cookies per site. To avoid showing a list
    // of '1 cookie', '1 cookie', ... etc, it is better to show the title of the
    // cookie to differentiate them.
    if (item.type == 'cookie')
      return item.title;
    return getCookieDataCategoryText(item.type, item.totalUsage);
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!Event} event
   * @private
   */
  onRemove_: function(event) {
    this.browserProxy.removeCookie(
        /** @type {!CookieDetails} */(event.currentTarget.dataset).idPath);
  },

  /**
   * A handler for when the user opts to remove all cookies.
   */
  removeAll: function() {
    this.browserProxy.removeCookie(this.siteId_);
  },
});

})();
