// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-search-page' is the settings page containing search settings.
 */
Polymer({
  is: 'settings-search-page',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * List of default search engines available.
     * @private {!Array<!SearchEngine>}
     */
    searchEngines_: {
      type: Array,
      value: function() { return []; }
    },

    /** @private {!settings.SearchEnginesBrowserProxy} */
    browserProxy_: Object,
  },

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    var updateSearchEngines = function(searchEngines) {
      this.set('searchEngines_', searchEngines.defaults);
    }.bind(this);
    this.browserProxy_.getSearchEnginesList().then(updateSearchEngines);
    cr.addWebUIListener('search-engines-changed', updateSearchEngines);
  },

  /** @private */
  onManageSearchEnginesTap_: function() {
    this.$.pages.setSubpageChain(['search-engines']);
  },

  /** @private */
  onIronSelect_: function() {
    var searchEngine = this.searchEngines_[this.$$('paper-listbox').selected];
    if (searchEngine.default) {
      // If the selected search engine is already marked as the default one,
      // this change originated in some other tab, and nothing should be done
      // here.
      return;
    }

    // Otherwise, this change originated by an explicit user action in this tab.
    // Submit the default search engine change.
    this.browserProxy_.setDefaultSearchEngine(searchEngine.modelIndex);
  },

  /**
   * @return {number}
   * @private
   */
  getSelectedSearchEngineIndex_: function() {
    return this.searchEngines_.findIndex(function(searchEngine) {
      return searchEngine.default;
    });
  },
});
