// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-url-dialog' is a component for adding
 * or editing a startup URL entry.
 */
Polymer({
  is: 'settings-startup-url-dialog',

  properties: {
    /** @private {string} */
    url_: String,

    /**
     * If specified the dialog acts as an "Edit page" dialog, otherwise as an
     * "Add new page" dialog.
     * @type {?StartupPageInfo}
     */
    model: Object,

    /** @private {string} */
    dialogTitle_: String,

    /** @private {string} */
    actionButtonText_: String,
  },

  /** @private {!settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();

    if (this.model) {
      this.dialogTitle_ = loadTimeData.getString('onStartupEditPage');
      this.actionButtonText_ = loadTimeData.getString('onStartupEdit');
      this.$.actionButton.disabled = false;
      // Pre-populate the input field.
      this.url_ = this.model.url;
    } else {
      this.dialogTitle_ = loadTimeData.getString('onStartupAddNewPage');
      this.actionButtonText_ = loadTimeData.getString('add');
      this.$.actionButton.disabled = true;
    }
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onActionButtonTap_: function() {
    var whenDone = this.model ?
        this.browserProxy_.editStartupPage(this.model.modelIndex, this.url_) :
        this.browserProxy_.addStartupPage(this.url_);

    whenDone.then(function(success) {
      if (success)
        this.$.dialog.close();
      // If the URL was invalid, there is nothing to do, just leave the dialog
      // open and let the user fix the URL or cancel.
    }.bind(this));
  },

  /** @private */
  validate_: function() {
    this.browserProxy_.validateStartupPage(this.url_).then(function(isValid) {
      this.$.actionButton.disabled = !isValid;
    }.bind(this));
  },
});
