// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// AddLanguageOverlay class:

/**
 * @typedef {{
 *   code: string,
 *   displayName: string,
 *   textDirection: string,
 *   nativeDisplayName: string
 * }}
 */
options.LanguageData;

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of ChromeOS add language overlay page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function AddLanguageOverlay() {
    Page.call(this, 'addLanguage',
              loadTimeData.getString('addButton'),
              'add-language-overlay-page');
  }

  cr.addSingletonGetter(AddLanguageOverlay);

  AddLanguageOverlay.prototype = {
    // Inherit AddLanguageOverlay from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      // Set up the cancel button.
      $('add-language-overlay-cancel-button').onclick = function(e) {
        PageManager.closeOverlay();
      };

      // Create the language list with which users can add a language.
      var addLanguageList = $('add-language-overlay-language-list');

      /**
       * @type {!Array<!options.LanguageData>}
       * @see chrome/browser/ui/webui/options/language_options_handler.cc
       */
      var languageListData = /** @type {!Array<!options.LanguageData>} */(
          loadTimeData.getValue('languageList'));
      for (var i = 0; i < languageListData.length; i++) {
        var language = languageListData[i];
        var displayText = language.displayName;
        // If the native name is different, add it.
        if (language.displayName != language.nativeDisplayName)
          displayText += ' - ' + language.nativeDisplayName;

        var option = cr.doc.createElement('option');
        option.value = language.code;
        option.textContent = displayText;
        addLanguageList.appendChild(option);
      }
    },
  };

  return {
    AddLanguageOverlay: AddLanguageOverlay
  };
});
