// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// AddLanguageOverlay class:

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of ChromeOS add language overlay page.
   * @constructor
   */
  function AddLanguageOverlay() {
    OptionsPage.call(this, 'addLanguage',
                     loadTimeData.getString('addButton'),
                     'add-language-overlay-page');
  }

  cr.addSingletonGetter(AddLanguageOverlay);

  AddLanguageOverlay.prototype = {
    // Inherit AddLanguageOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initializes AddLanguageOverlay page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Set up the cancel button.
      $('add-language-overlay-cancel-button').onclick = function(e) {
        OptionsPage.closeOverlay();
      };

      // Create the language list with which users can add a language.
      var addLanguageList = $('add-language-overlay-language-list');
      var languageListData = loadTimeData.getValue('languageList');
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
