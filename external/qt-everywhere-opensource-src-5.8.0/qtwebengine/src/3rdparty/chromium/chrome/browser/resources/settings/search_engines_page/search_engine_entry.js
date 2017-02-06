// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-entry' is a component for showing a
 * search engine with its name, domain and query URL.
 */
Polymer({
  is: 'settings-search-engine-entry',

  properties: {
    /** @type {!SearchEngine} */
    engine: Object,

    /** @private {boolean} */
    showEditSearchEngineDialog_: Boolean,

    /** @type {boolean} */
    isDefault: {
      reflectToAttribute: true,
      type: Boolean,
      computed: 'computeIsDefault_(engine)'
    },
  },

  /** @private {settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsDefault_: function() {
    return this.engine.default;
  },

  /** @private */
  onDeleteTap_: function() {
    this.browserProxy_.removeSearchEngine(this.engine.modelIndex);
  },

  /** @private */
  onEditTap_: function() {
    this.closePopupMenu_();

    this.showEditSearchEngineDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-search-engine-dialog');
      // Register listener to detect when the dialog is closed. Flip the boolean
      // once closed to force a restamp next time it is shown such that the
      // previous dialog's contents are cleared.
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showEditSearchEngineDialog_ = false;
      }.bind(this));
    }.bind(this));
  },

  /** @private */
  onMakeDefaultTap_: function() {
    this.closePopupMenu_();
    this.browserProxy_.setDefaultSearchEngine(this.engine.modelIndex);
  },

  /** @private */
  closePopupMenu_: function() {
    this.$$('iron-dropdown').close();
  },

  /**
   * @param {?string} url The icon URL if available.
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    // Force default icon, if no |engine.iconURL| is available.
    return cr.icon.getFaviconImageSet(url || '');
  },
});
