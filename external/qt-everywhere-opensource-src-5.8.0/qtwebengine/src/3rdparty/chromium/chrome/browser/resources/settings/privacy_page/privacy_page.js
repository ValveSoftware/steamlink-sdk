// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */
Polymer({
  is: 'settings-privacy-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** @private */
    showClearBrowsingDataDialog_: {
      computed: 'computeShowClearBrowsingDataDialog_(currentRoute)',
      type: Boolean,
    },
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
  },

  /** @suppress {missingProperties} */
  attached: function() {
    settings.main.rendered.then(function() {
      if (this.showClearBrowsingDataDialog_) {
        var dialog = this.$$('settings-clear-browsing-data-dialog').$.dialog;
        // TODO(dbeam): cast to a CrDialogElement when it compiles.
        dialog.refit();
      }
    }.bind(this));
  },

  /**
   * @return {boolean} Whether the Clear Browsing Data dialog should be showing.
   * @private
   */
  computeShowClearBrowsingDataDialog_: function() {
    var route = this.currentRoute;
    return route && route.dialog == 'clear-browsing-data';
  },

  /** @private */
  onManageCertificatesTap_: function() {
<if expr="use_nss_certs">
    var pages = /** @type {!SettingsAnimatedPagesElement} */(this.$.pages);
    pages.setSubpageChain(['manage-certificates']);
</if>
<if expr="is_win or is_macosx">
    settings.PrivacyPageBrowserProxyImpl.getInstance().
      showManageSSLCertificates();
</if>
  },

  /** @private */
  onSiteSettingsTap_: function() {
    var pages = /** @type {!SettingsAnimatedPagesElement} */(this.$.pages);
    pages.setSubpageChain(['site-settings']);
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    this.currentRoute = {
      page: this.currentRoute.page,
      section: this.currentRoute.section,
      subpage: this.currentRoute.subpage,
      dialog: 'clear-browsing-data',
    };
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronOverlayClosed_: function(event) {
    if (Polymer.dom(event).rootTarget.tagName != 'CR-DIALOG')
      return;

    this.currentRoute = {
      page: this.currentRoute.page,
      section: this.currentRoute.section,
      subpage: this.currentRoute.subpage,
      // Drop dialog key.
    };
  },
});
