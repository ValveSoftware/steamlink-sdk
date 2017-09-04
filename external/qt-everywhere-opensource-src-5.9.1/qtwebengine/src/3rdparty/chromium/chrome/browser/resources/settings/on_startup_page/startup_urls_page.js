// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

Polymer({
  is: 'settings-startup-urls-page',

  behaviors: [CrScrollableBehavior, WebUIListenerBehavior],

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
      // If an "edit" URL dialog was open, close it, because the underlying page
      // might have just been removed (and model indices have changed anyway).
      if (this.startupUrlDialogModel_)
        this.destroyUrlDialog_();
      this.startupPages_ = startupPages;
      this.updateScrollableContents();
    }.bind(this));
    this.browserProxy_.loadStartupPages();

    this.addEventListener(settings.EDIT_STARTUP_URL_EVENT, function(event) {
      this.startupUrlDialogModel_ = event.detail;
      this.showStartupUrlDialog_ = true;
      event.stopPropagation();
    }.bind(this));
  },

  /** @private */
  onAddPageTap_: function() {
    this.showStartupUrlDialog_ = true;
  },

  /** @private */
  destroyUrlDialog_: function() {
    this.showStartupUrlDialog_ = false;
    this.startupUrlDialogModel_ = null;
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    this.browserProxy_.useCurrentPages();
  },
});
