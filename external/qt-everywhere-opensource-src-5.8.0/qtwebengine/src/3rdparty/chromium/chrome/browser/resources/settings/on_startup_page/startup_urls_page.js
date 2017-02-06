// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

Polymer({
  is: 'settings-startup-urls-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {settings.StartupUrlsPageBrowserProxy} */
    browserProxy_: Object,

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,

    /** @private */
    showStartupUrlDialog_: Boolean,

    /** @private {?StartupPageInfo} */
    startupUrlDialogModel_: Object,
  },

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();
    this.addWebUIListener('update-startup-pages', function(startupPages) {
      this.startupPages_ = startupPages;
    }.bind(this));
    this.browserProxy_.loadStartupPages();

    this.addEventListener(settings.EDIT_STARTUP_URL_EVENT, function(event) {
      this.startupUrlDialogModel_ = event.detail;
      this.openDialog_();
      event.stopPropagation();
    }.bind(this));
  },

  /** @private */
  onAddPageTap_: function() {
    this.openDialog_();
  },

  /**
   * Opens the dialog and registers a listener for removing the dialog from the
   * DOM once is closed. The listener is destroyed when the dialog is removed
   * (because of 'restamp').
   * @private
   */
  openDialog_: function() {
    this.showStartupUrlDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-startup-url-dialog');
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showStartupUrlDialog_ = false;
        this.startupUrlDialogModel_ = null;
      }.bind(this));
    }.bind(this));
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    this.browserProxy_.useCurrentPages();
  },
});
