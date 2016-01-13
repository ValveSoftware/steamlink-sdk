// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * StartupOverlay class
   * Encapsulated handling of the 'Set Startup pages' overlay page.
   * @constructor
   * @class
   */
  function StartupOverlay() {
    SettingsDialog.call(this, 'startup',
                        loadTimeData.getString('startupPagesOverlayTabTitle'),
                        'startup-overlay',
                        $('startup-overlay-confirm'),
                        $('startup-overlay-cancel'));
  };

  cr.addSingletonGetter(StartupOverlay);

  StartupOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * An autocomplete list that can be attached to a text field during editing.
     * @type {HTMLElement}
     * @private
     */
    autocompleteList_: null,

    startup_pages_pref_: {
      'name': 'session.startup_urls',
      'disabled': false
    },

    /**
     * Initialize the page.
     */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);

      var self = this;

      var startupPagesList = $('startupPagesList');
      options.browser_options.StartupPageList.decorate(startupPagesList);
      startupPagesList.autoExpands = true;

      $('startupUseCurrentButton').onclick = function(event) {
        chrome.send('setStartupPagesToCurrentPages');
      };

      Preferences.getInstance().addEventListener(
          this.startup_pages_pref_.name,
          this.handleStartupPageListChange_.bind(this));

      var suggestionList = new cr.ui.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.requestSuggestions =
          this.requestAutocompleteSuggestions_.bind(this);
      $('startup-overlay').appendChild(suggestionList);
      this.autocompleteList_ = suggestionList;
      startupPagesList.autocompleteList = suggestionList;
    },

    /** @override */
    handleConfirm: function() {
      SettingsDialog.prototype.handleConfirm.call(this);
      chrome.send('commitStartupPrefChanges');
      // Set the startup behavior to "open specific set of pages" so that the
      // pages the user selected actually get opened on startup.
      Preferences.setIntegerPref('session.restore_on_startup', 4, true);
    },

    /** @override */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      chrome.send('cancelStartupPrefChanges');
    },

    /**
     * Sets the enabled state of the custom startup page list
     * @param {boolean} disable True to disable, false to enable
     */
    setControlsDisabled: function(disable) {
      var startupPagesList = $('startupPagesList');
      startupPagesList.disabled = disable;
      startupPagesList.setAttribute('tabindex', disable ? -1 : 0);
      // Explicitly set disabled state for input text elements.
      var inputs = startupPagesList.querySelectorAll("input[type='text']");
      for (var i = 0; i < inputs.length; i++)
        inputs[i].disabled = disable;
      $('startupUseCurrentButton').disabled = disable;
    },

    /**
     * Enables or disables the the custom startup page list controls
     * based on the whether the 'pages to restore on startup' pref is enabled.
     */
    updateControlStates: function() {
      this.setControlsDisabled(
          this.startup_pages_pref_.disabled);
    },

    /**
     * Handles change events of the preference
     * 'session.startup_urls'.
     * @param {event} preference changed event.
     * @private
     */
    handleStartupPageListChange_: function(event) {
      this.startup_pages_pref_.disabled = event.value.disabled;
      this.updateControlStates();
    },

    /**
     * Updates the startup pages list with the given entries.
     * @param {Array} pages List of startup pages.
     * @private
     */
    updateStartupPages_: function(pages) {
      var model = new ArrayDataModel(pages);
      // Add a "new page" row.
      model.push({modelIndex: -1});
      $('startupPagesList').dataModel = model;
    },

    /**
     * Sends an asynchronous request for new autocompletion suggestions for the
     * the given query. When new suggestions are available, the C++ handler will
     * call updateAutocompleteSuggestions_.
     * @param {string} query List of autocomplete suggestions.
     * @private
     */
    requestAutocompleteSuggestions_: function(query) {
      chrome.send('requestAutocompleteSuggestionsForStartupPages', [query]);
    },

    /**
     * Updates the autocomplete suggestion list with the given entries.
     * @param {Array} pages List of autocomplete suggestions.
     * @private
     */
    updateAutocompleteSuggestions_: function(suggestions) {
      var list = this.autocompleteList_;
      // If the trigger for this update was a value being selected from the
      // current list, do nothing.
      if (list.targetInput && list.selectedItem &&
          list.selectedItem.url == list.targetInput.value) {
        return;
      }
      list.suggestions = suggestions;
    },
  };

  // Forward public APIs to private implementations.
  [
    'updateStartupPages',
    'updateAutocompleteSuggestions',
  ].forEach(function(name) {
    StartupOverlay[name] = function() {
      var instance = StartupOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    StartupOverlay: StartupOverlay
  };
});
