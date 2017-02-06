// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * HomePageOverlay class
   * Dialog that allows users to set the home page.
   * @constructor
   * @extends {options.SettingsDialog}
   */
  function HomePageOverlay() {
    SettingsDialog.call(this, 'homePageOverlay',
        loadTimeData.getString('homePageOverlayTabTitle'),
        'home-page-overlay',
        assertInstanceof($('home-page-confirm'), HTMLButtonElement),
        assertInstanceof($('home-page-cancel'), HTMLButtonElement));
  }

  cr.addSingletonGetter(HomePageOverlay);

  HomePageOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * An autocomplete list that can be attached to the home page URL field.
     * @type {cr.ui.AutocompleteList}
     * @private
     */
    autocompleteList_: null,

    /** @override */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);

      var self = this;
      options.Preferences.getInstance().addEventListener(
          'homepage_is_newtabpage',
          this.handleHomepageIsNTPPrefChange.bind(this));

      var urlField = $('homepage-url-field');
      urlField.addEventListener('keydown', function(event) {
        // Don't auto-submit when the user selects something from the
        // auto-complete list.
        if (event.key == 'Enter' && !self.autocompleteList_.hidden)
          event.stopPropagation();
      });
      urlField.addEventListener('change', this.updateFavicon_.bind(this));

      var suggestionList = new cr.ui.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.requestSuggestions =
          this.requestAutocompleteSuggestions_.bind(this);
      $('home-page-overlay').appendChild(suggestionList);
      this.autocompleteList_ = suggestionList;

      urlField.addEventListener('focus', function(event) {
        self.autocompleteList_.attachToInput(urlField);
      });
      urlField.addEventListener('blur', function(event) {
        self.autocompleteList_.detach();
      });
    },

    /** @override */
    didShowPage: function() {
      this.updateFavicon_();
    },

    /**
     * Updates the state of the homepage text input and its controlled setting
     * indicator when the |homepage_is_newtabpage| pref changes. The input is
     * enabled only if the homepage is not the NTP. The indicator is always
     * enabled but treats the input's value as read-only if the homepage is the
     * NTP.
     * @param {Event} event Pref change event.
     */
    handleHomepageIsNTPPrefChange: function(event) {
      var urlField = $('homepage-url-field');
      var urlFieldIndicator = $('homepage-url-field-indicator');
      urlField.setDisabled('homepage-is-ntp', event.value.value);
      urlFieldIndicator.readOnly = event.value.value;
    },

    /**
     * Updates the background of the url field to show the favicon for the
     * URL that is currently typed in.
     * @private
     */
    updateFavicon_: function() {
      var urlField = $('homepage-url-field');
      urlField.style.backgroundImage = cr.icon.getFaviconImageSet(
          urlField.value);
    },

    /**
     * Sends an asynchronous request for new autocompletion suggestions for the
     * the given query. When new suggestions are available, the C++ handler will
     * call updateAutocompleteSuggestions_.
     * @param {string} query List of autocomplete suggestions.
     * @private
     */
    requestAutocompleteSuggestions_: function(query) {
      chrome.send('requestAutocompleteSuggestionsForHomePage', [query]);
    },

    /**
     * Updates the autocomplete suggestion list with the given entries.
     * @param {Array} suggestions List of autocomplete suggestions.
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

    /**
     * Sets the 'show home button' and 'home page is new tab page' preferences.
     * (The home page url preference is set automatically by the SettingsDialog
     * code.)
     */
    handleConfirm: function() {
      // Strip whitespace.
      var urlField = $('homepage-url-field');
      var homePageValue = urlField.value.replace(/\s*/g, '');
      urlField.value = homePageValue;

      // Don't save an empty URL for the home page. If the user left the field
      // empty, switch to the New Tab page.
      if (!homePageValue)
        $('homepage-use-ntp').checked = true;

      SettingsDialog.prototype.handleConfirm.call(this);
    },
  };

  HomePageOverlay.updateAutocompleteSuggestions = function() {
    var instance = HomePageOverlay.getInstance();
    instance.updateAutocompleteSuggestions_.apply(instance, arguments);
  };

  // Export
  return {
    HomePageOverlay: HomePageOverlay
  };
});
