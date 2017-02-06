// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engines-page' is the settings page
 * containing search engines settings.
 */
Polymer({
  is: 'settings-search-engines-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {!Array<!SearchEngine>} */
    defaultEngines: {
      type: Array,
      value: function() { return []; }
    },

    /** @type {!Array<!SearchEngine>} */
    otherEngines: {
      type: Array,
      value: function() { return []; }
    },

    /** @type {!Array<!SearchEngine>} */
    extensions: {
      type: Array,
      value: function() { return []; }
    },

    /** @private {boolean} */
    showAddSearchEngineDialog_: Boolean,

    /** @private {boolean} */
    showExtensionsList_: {
      type: Boolean,
      computed: 'computeShowExtensionsList_(extensions)',
    }
  },

  /** @override */
  ready: function() {
    settings.SearchEnginesBrowserProxyImpl.getInstance().
        getSearchEnginesList().then(this.enginesChanged_.bind(this));
    this.addWebUIListener(
        'search-engines-changed', this.enginesChanged_.bind(this));
  },

  /**
   * @param {!SearchEnginesInfo} searchEnginesInfo
   * @private
   */
  enginesChanged_: function(searchEnginesInfo) {
    this.defaultEngines = searchEnginesInfo['defaults'];
    this.otherEngines = searchEnginesInfo['others'];
    this.extensions = searchEnginesInfo['extensions'];
  },

  /** @private */
  onAddSearchEngineTap_: function() {
    this.showAddSearchEngineDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-search-engine-dialog');
      // Register listener to detect when the dialog is closed. Flip the boolean
      // once closed to force a restamp next time it is shown such that the
      // previous dialog's contents are cleared.
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showAddSearchEngineDialog_ = false;
      }.bind(this));
    }.bind(this));
  },

  /** @private */
  computeShowExtensionsList_: function() {
    return this.extensions.length > 0;
  },
});
