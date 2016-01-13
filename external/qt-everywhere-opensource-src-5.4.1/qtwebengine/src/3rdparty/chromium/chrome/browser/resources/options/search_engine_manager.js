// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Encapsulated handling of search engine management page.
   * @constructor
   */
  function SearchEngineManager() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'searchEngines',
                     loadTimeData.getString('searchEngineManagerPageTabTitle'),
                     'search-engine-manager-page');
  }

  cr.addSingletonGetter(SearchEngineManager);

  SearchEngineManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * List for default search engine options.
     * @private
     */
    defaultsList_: null,

    /**
     * List for other search engine options.
     * @private
     */
    othersList_: null,

    /**
     * List for extension keywords.
     * @private
     */
    extensionList_: null,

    /** inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.defaultsList_ = $('default-search-engine-list');
      this.setUpList_(this.defaultsList_);

      this.othersList_ = $('other-search-engine-list');
      this.setUpList_(this.othersList_);

      this.extensionList_ = $('extension-keyword-list');
      this.setUpList_(this.extensionList_);

      $('search-engine-manager-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };
    },

    /**
     * Sets up the given list as a search engine list
     * @param {List} list The list to set up.
     * @private
     */
    setUpList_: function(list) {
      options.search_engines.SearchEngineList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the search engine list with the given entries.
     * @private
     * @param {Array} defaultEngines List of possible default search engines.
     * @param {Array} otherEngines List of other search engines.
     * @param {Array} keywords List of keywords from extensions.
     */
    updateSearchEngineList_: function(defaultEngines, otherEngines, keywords) {
      this.defaultsList_.dataModel = new ArrayDataModel(defaultEngines);

      otherEngines = otherEngines.map(function(x) {
        return [x, x.name.toLocaleLowerCase()];
      }).sort(function(a, b) {
        return a[1].localeCompare(b[1]);
      }).map(function(x) {
        return x[0];
      });

      var othersModel = new ArrayDataModel(otherEngines);
      // Add a "new engine" row.
      othersModel.push({
        'modelIndex': '-1',
        'canBeEdited': true
      });
      this.othersList_.dataModel = othersModel;

      if (keywords.length > 0) {
        $('extension-keyword-div').hidden = false;
        var extensionsModel = new ArrayDataModel(keywords);
        this.extensionList_.dataModel = extensionsModel;
      } else {
        $('extension-keyword-div').hidden = true;
      }
    },
  };

  SearchEngineManager.updateSearchEngineList = function(defaultEngines,
                                                        otherEngines,
                                                        keywords) {
    SearchEngineManager.getInstance().updateSearchEngineList_(defaultEngines,
                                                              otherEngines,
                                                              keywords);
  };

  SearchEngineManager.validityCheckCallback = function(validity, modelIndex) {
    // Forward to all lists; those without a matching modelIndex will ignore it.
    SearchEngineManager.getInstance().defaultsList_.validationComplete(
        validity, modelIndex);
    SearchEngineManager.getInstance().othersList_.validationComplete(
        validity, modelIndex);
    SearchEngineManager.getInstance().extensionList_.validationComplete(
        validity, modelIndex);
  };

  // Export
  return {
    SearchEngineManager: SearchEngineManager
  };

});

