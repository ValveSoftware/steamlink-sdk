// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

Polymer({
  is: 'site-data',

  behaviors: [CookieTreeBehavior],

  properties: {
    /**
     * The current filter applied to the cookie data list.
     * @private
     */
    filterString_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmationDeleteMsg_: String,

    /** @private */
    idToDelete_: String,
  },

  /** @override */
  ready: function() {
    this.loadCookies();
  },

  /**
   * A filter function for the list.
   * @param {!CookieDataSummaryItem} item The item to possibly filter out.
   * @return {boolean} Whether to show the item.
   * @private
   */
  showItem_: function(item) {
    if (this.filterString_.length == 0)
      return true;
    return item.site.indexOf(this.filterString_) > -1;
  },

  /** @private */
  onSearchChanged_: function(e) {
    this.filterString_ = e.detail;
    this.$.list.render();
  },

  /** @private */
  isRemoveButtonVisible_: function(sites, renderedItemCount) {
    return renderedItemCount != 0;
  },

  /**
   * Returns the string to use for the Remove label.
   * @return {string} filterString The current filter string.
   * @private
   */
  computeRemoveLabel_: function(filterString) {
    if (filterString.length == 0)
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   * @private
   */
  onConfirmDeleteMultipleSites_: function() {
    this.idToDelete_ = '';  // Delete all.
    this.confirmationDeleteMsg_ = loadTimeData.getString(
        'siteSettingsCookieRemoveMultipleConfirmation');
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Called when deletion for a single/multiple sites has been confirmed.
   * @private
   */
  onConfirmDelete_: function() {
    if (this.idToDelete_ != '')
      this.onDeleteSite_();
    else
      this.onDeleteMultipleSites_();
    this.$.confirmDeleteDialog.close();
  },

  /**
   * Deletes all site data for a given site.
   * @private
   */
  onDeleteSite_: function() {
    this.browserProxy.removeCookie(this.idToDelete_);
  },

  /**
   * Deletes site data for multiple sites.
   * @private
   */
  onDeleteMultipleSites_: function() {
    if (this.filterString_.length == 0) {
      this.removeAllCookies();
    } else {
      var items = this.$.list.items;
      for (var i = 0; i < items.length; ++i) {
        if (this.showItem_(items[i]))
          this.browserProxy.removeCookie(items[i].id);
      }
      // We just deleted all items found by the filter, let's reset the filter.
      /** @type {SettingsSubpageSearchElement} */(this.$.filter).setValue('');
    }
  },

  /**
   * @param {!{model: !{item: CookieDataSummaryItem}}} event
   * @private
   */
  onSiteTap_: function(event) {
    settings.navigateTo(settings.Route.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + event.model.item.site));
  },
});
