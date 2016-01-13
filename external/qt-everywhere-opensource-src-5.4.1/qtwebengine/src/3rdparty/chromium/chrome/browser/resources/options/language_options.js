// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kochi): Generalize the notification as a component and put it
// in js/cr/ui/notification.js .

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var LanguageList = options.LanguageList;
  /** @const */ var ThirdPartyImeConfirmOverlay =
      options.ThirdPartyImeConfirmOverlay;

  /**
   * Spell check dictionary download status.
   * @type {Enum}
   */
  /** @const*/ var DOWNLOAD_STATUS = {
    IN_PROGRESS: 1,
    FAILED: 2
  };

  /**
   * The preference is a boolean that enables/disables spell checking.
   * @type {string}
   * @const
   */
  var ENABLE_SPELL_CHECK_PREF = 'browser.enable_spellchecking';

  /**
   * The preference is a CSV string that describes preload engines
   * (i.e. active input methods).
   * @type {string}
   * @const
   */
  var PRELOAD_ENGINES_PREF = 'settings.language.preload_engines';

  /**
   * The preference that lists the extension IMEs that are enabled in the
   * language menu.
   * @type {string}
   * @const
   */
  var ENABLED_EXTENSION_IME_PREF = 'settings.language.enabled_extension_imes';

  /**
   * The preference that lists the languages which are not translated.
   * @type {string}
   * @const
   */
  var TRANSLATE_BLOCKED_LANGUAGES_PREF = 'translate_blocked_languages';

  /**
   * The preference key that is a string that describes the spell check
   * dictionary language, like "en-US".
   * @type {string}
   * @const
   */
  var SPELL_CHECK_DICTIONARY_PREF = 'spellcheck.dictionary';

  /**
   * The preference that indicates if the Translate feature is enabled.
   * @type {string}
   * @const
   */
  var ENABLE_TRANSLATE = 'translate.enabled';

  /////////////////////////////////////////////////////////////////////////////
  // LanguageOptions class:

  /**
   * Encapsulated handling of ChromeOS language options page.
   * @constructor
   */
  function LanguageOptions(model) {
    OptionsPage.call(this, 'languages',
                     loadTimeData.getString('languagePageTabTitle'),
                     'languagePage');
  }

  cr.addSingletonGetter(LanguageOptions);

  // Inherit LanguageOptions from OptionsPage.
  LanguageOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /* For recording the prospective language (the next locale after relaunch).
     * @type {?string}
     * @private
     */
    prospectiveUiLanguageCode_: null,

    /*
     * Map from language code to spell check dictionary download status for that
     * language.
     * @type {Array}
     * @private
     */
    spellcheckDictionaryDownloadStatus_: [],

    /**
     * Number of times a spell check dictionary download failed.
     * @type {int}
     * @private
     */
    spellcheckDictionaryDownloadFailures_: 0,

    /**
     * The list of preload engines, like ['mozc', 'pinyin'].
     * @type {Array}
     * @private
     */
    preloadEngines_: [],

    /**
     * The list of extension IMEs that are enabled out of the language menu.
     * @type {Array}
     * @private
     */
    enabledExtensionImes_: [],

    /**
     * The list of the languages which is not translated.
     * @type {Array}
     * @private
     */
    translateBlockedLanguages_: [],

    /**
     * The list of the languages supported by Translate server
     * @type {Array}
     * @private
     */
    translateSupportedLanguages_: [],

    /**
     * The preference is a string that describes the spell check dictionary
     * language, like "en-US".
     * @type {string}
     * @private
     */
    spellCheckDictionary_: '',

    /**
     * The map of language code to input method IDs, like:
     * {'ja': ['mozc', 'mozc-jp'], 'zh-CN': ['pinyin'], ...}
     * @type {Object}
     * @private
     */
    languageCodeToInputMethodIdsMap_: {},

    /**
     * The value that indicates if Translate feature is enabled or not.
     * @type {boolean}
     * @private
     */
    enableTranslate_: false,

    /**
     * Initializes LanguageOptions page.
     * Calls base class implementation to start preference initialization.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var languageOptionsList = $('language-options-list');
      LanguageList.decorate(languageOptionsList);

      languageOptionsList.addEventListener('change',
          this.handleLanguageOptionsListChange_.bind(this));
      languageOptionsList.addEventListener('save',
          this.handleLanguageOptionsListSave_.bind(this));

      this.prospectiveUiLanguageCode_ =
          loadTimeData.getString('prospectiveUiLanguageCode');
      this.addEventListener('visibleChange',
                            this.handleVisibleChange_.bind(this));

      if (cr.isChromeOS) {
        this.initializeInputMethodList_();
        this.initializeLanguageCodeToInputMethodIdsMap_();
      }

      var checkbox = $('offer-to-translate-in-this-language');
      checkbox.addEventListener('click',
          this.handleOfferToTranslateCheckboxClick_.bind(this));

      Preferences.getInstance().addEventListener(
          TRANSLATE_BLOCKED_LANGUAGES_PREF,
          this.handleTranslateBlockedLanguagesPrefChange_.bind(this));
      Preferences.getInstance().addEventListener(SPELL_CHECK_DICTIONARY_PREF,
          this.handleSpellCheckDictionaryPrefChange_.bind(this));
      Preferences.getInstance().addEventListener(ENABLE_TRANSLATE,
          this.handleEnableTranslatePrefChange_.bind(this));
      this.translateSupportedLanguages_ =
          loadTimeData.getValue('translateSupportedLanguages');

      // Set up add button.
      $('language-options-add-button').onclick = function(e) {
        // Add the language without showing the overlay if it's specified in
        // the URL hash (ex. lang_add=ja).  Used for automated testing.
        var match = document.location.hash.match(/\blang_add=([\w-]+)/);
        if (match) {
          var addLanguageCode = match[1];
          $('language-options-list').addLanguage(addLanguageCode);
          this.addBlockedLanguage_(addLanguageCode);
        } else {
          OptionsPage.navigateToPage('addLanguage');
        }
      }.bind(this);

      if (!cr.isMac) {
        // Set up the button for editing custom spelling dictionary.
        $('edit-dictionary-button').onclick = function(e) {
          OptionsPage.navigateToPage('editDictionary');
        };
        $('dictionary-download-retry-button').onclick = function(e) {
          chrome.send('retryDictionaryDownload');
        };
      }

      // Listen to add language dialog ok button.
      $('add-language-overlay-ok-button').addEventListener(
          'click', this.handleAddLanguageOkButtonClick_.bind(this));

      if (!cr.isChromeOS) {
        // Show experimental features if enabled.
        if (loadTimeData.getBoolean('enableSpellingAutoCorrect'))
          $('auto-spell-correction-option').hidden = false;

        // Handle spell check enable/disable.
        if (!cr.isMac) {
          Preferences.getInstance().addEventListener(
              ENABLE_SPELL_CHECK_PREF,
              this.updateEnableSpellCheck_.bind(this));
        }
      }

      // Handle clicks on "Use this language for spell checking" button.
      if (!cr.isMac) {
        var spellCheckLanguageButton = getRequiredElement(
            'language-options-spell-check-language-button');
        spellCheckLanguageButton.addEventListener(
            'click',
            this.handleSpellCheckLanguageButtonClick_.bind(this));
      }

      if (cr.isChromeOS) {
        $('language-options-ui-restart-button').onclick = function() {
          chrome.send('uiLanguageRestart');
        };
      }

      $('language-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /**
     * Initializes the input method list.
     */
    initializeInputMethodList_: function() {
      var inputMethodList = $('language-options-input-method-list');
      var inputMethodPrototype = $('language-options-input-method-template');

      // Add all input methods, but make all of them invisible here. We'll
      // change the visibility in handleLanguageOptionsListChange_() based
      // on the selected language. Note that we only have less than 100
      // input methods, so creating DOM nodes at once here should be ok.
      this.appendInputMethodElement_(loadTimeData.getValue('inputMethodList'));
      this.appendInputMethodElement_(loadTimeData.getValue('extensionImeList'));
      this.appendComponentExtensionIme_(
          loadTimeData.getValue('componentExtensionImeList'));

      // Listen to pref change once the input method list is initialized.
      Preferences.getInstance().addEventListener(
          PRELOAD_ENGINES_PREF,
          this.handlePreloadEnginesPrefChange_.bind(this));
      Preferences.getInstance().addEventListener(
          ENABLED_EXTENSION_IME_PREF,
          this.handleEnabledExtensionsPrefChange_.bind(this));
    },

    /**
     * Appends input method lists based on component extension ime list.
     * @param {!Array} componentExtensionImeList A list of input method
     *     descriptors.
     * @private
     */
    appendComponentExtensionIme_: function(componentExtensionImeList) {
      this.appendInputMethodElement_(componentExtensionImeList);

      for (var i = 0; i < componentExtensionImeList.length; i++) {
        var inputMethod = componentExtensionImeList[i];
        for (var languageCode in inputMethod.languageCodeSet) {
          if (languageCode in this.languageCodeToInputMethodIdsMap_) {
            this.languageCodeToInputMethodIdsMap_[languageCode].push(
                inputMethod.id);
          } else {
            this.languageCodeToInputMethodIdsMap_[languageCode] =
                [inputMethod.id];
          }
        }
      }
    },

    /**
     * Appends input methods into input method list.
     * @param {!Array} inputMethods A list of input method descriptors.
     * @private
     */
    appendInputMethodElement_: function(inputMethods) {
      var inputMethodList = $('language-options-input-method-list');
      var inputMethodTemplate = $('language-options-input-method-template');

      for (var i = 0; i < inputMethods.length; i++) {
        var inputMethod = inputMethods[i];
        var element = inputMethodTemplate.cloneNode(true);
        element.id = '';
        element.languageCodeSet = inputMethod.languageCodeSet;

        var input = element.querySelector('input');
        input.inputMethodId = inputMethod.id;
        input.imeProvider = inputMethod.extensionName;
        var span = element.querySelector('span');
        span.textContent = inputMethod.displayName;

        if (inputMethod.optionsPage) {
          var button = document.createElement('button');
          button.textContent = loadTimeData.getString('configure');
          button.inputMethodId = inputMethod.id;
          button.onclick = function(inputMethodId, e) {
            chrome.send('inputMethodOptionsOpen', [inputMethodId]);
          }.bind(this, inputMethod.id);
          element.appendChild(button);
        }

        // Listen to user clicks.
        input.addEventListener('click',
                               this.handleCheckboxClick_.bind(this));
        inputMethodList.appendChild(element);
      }
    },

    /**
     * Adds a language to the preference 'translate_blocked_languages'. If
     * |langCode| is already added, nothing happens. |langCode| is converted
     * to a Translate language synonym before added.
     * @param {string} langCode A language code like 'en'
     * @private
     */
    addBlockedLanguage_: function(langCode) {
      langCode = this.convertLangCodeForTranslation_(langCode);
      if (this.translateBlockedLanguages_.indexOf(langCode) == -1) {
        this.translateBlockedLanguages_.push(langCode);
        Preferences.setListPref(TRANSLATE_BLOCKED_LANGUAGES_PREF,
                                this.translateBlockedLanguages_, true);
      }
    },

    /**
     * Removes a language from the preference 'translate_blocked_languages'.
     * If |langCode| doesn't exist in the preference, nothing happens.
     * |langCode| is converted to a Translate language synonym before removed.
     * @param {string} langCode A language code like 'en'
     * @private
     */
    removeBlockedLanguage_: function(langCode) {
      langCode = this.convertLangCodeForTranslation_(langCode);
      if (this.translateBlockedLanguages_.indexOf(langCode) != -1) {
        this.translateBlockedLanguages_ =
            this.translateBlockedLanguages_.filter(
                function(langCodeNotTranslated) {
                  return langCodeNotTranslated != langCode;
                });
        Preferences.setListPref(TRANSLATE_BLOCKED_LANGUAGES_PREF,
                                this.translateBlockedLanguages_, true);
      }
    },

    /**
     * Handles OptionsPage's visible property change event.
     * @param {Event} e Property change event.
     * @private
     */
    handleVisibleChange_: function(e) {
      if (this.visible) {
        $('language-options-list').redraw();
        chrome.send('languageOptionsOpen');
      }
    },

    /**
     * Handles languageOptionsList's change event.
     * @param {Event} e Change event.
     * @private
     */
    handleLanguageOptionsListChange_: function(e) {
      var languageOptionsList = $('language-options-list');
      var languageCode = languageOptionsList.getSelectedLanguageCode();

      // If there's no selection, just return.
      if (!languageCode)
        return;

      // Select the language if it's specified in the URL hash (ex. lang=ja).
      // Used for automated testing.
      var match = document.location.hash.match(/\blang=([\w-]+)/);
      if (match) {
        var specifiedLanguageCode = match[1];
        if (languageOptionsList.selectLanguageByCode(specifiedLanguageCode)) {
          languageCode = specifiedLanguageCode;
        }
      }

      this.updateOfferToTranslateCheckbox_(languageCode);

      if (cr.isWindows || cr.isChromeOS)
        this.updateUiLanguageButton_(languageCode);

      this.updateSelectedLanguageName_(languageCode);

      if (!cr.isMac)
        this.updateSpellCheckLanguageButton_(languageCode);

      if (cr.isChromeOS)
        this.updateInputMethodList_(languageCode);

      this.updateLanguageListInAddLanguageOverlay_();
    },

    /**
     * Handles languageOptionsList's save event.
     * @param {Event} e Save event.
     * @private
     */
    handleLanguageOptionsListSave_: function(e) {
      if (cr.isChromeOS) {
        // Sort the preload engines per the saved languages before save.
        this.preloadEngines_ = this.sortPreloadEngines_(this.preloadEngines_);
        this.savePreloadEnginesPref_();
      }
    },

    /**
     * Sorts preloadEngines_ by languageOptionsList's order.
     * @param {Array} preloadEngines List of preload engines.
     * @return {Array} Returns sorted preloadEngines.
     * @private
     */
    sortPreloadEngines_: function(preloadEngines) {
      // For instance, suppose we have two languages and associated input
      // methods:
      //
      // - Korean: hangul
      // - Chinese: pinyin
      //
      // The preloadEngines preference should look like "hangul,pinyin".
      // If the user reverse the order, the preference should be reorderd
      // to "pinyin,hangul".
      var languageOptionsList = $('language-options-list');
      var languageCodes = languageOptionsList.getLanguageCodes();

      // Convert the list into a dictonary for simpler lookup.
      var preloadEngineSet = {};
      for (var i = 0; i < preloadEngines.length; i++) {
        preloadEngineSet[preloadEngines[i]] = true;
      }

      // Create the new preload engine list per the language codes.
      var newPreloadEngines = [];
      for (var i = 0; i < languageCodes.length; i++) {
        var languageCode = languageCodes[i];
        var inputMethodIds = this.languageCodeToInputMethodIdsMap_[
            languageCode];
        if (!inputMethodIds)
          continue;

        // Check if we have active input methods associated with the language.
        for (var j = 0; j < inputMethodIds.length; j++) {
          var inputMethodId = inputMethodIds[j];
          if (inputMethodId in preloadEngineSet) {
            // If we have, add it to the new engine list.
            newPreloadEngines.push(inputMethodId);
            // And delete it from the set. This is necessary as one input
            // method can be associated with more than one language thus
            // we should avoid having duplicates in the new list.
            delete preloadEngineSet[inputMethodId];
          }
        }
      }

      return newPreloadEngines;
    },

    /**
     * Initializes the map of language code to input method IDs.
     * @private
     */
    initializeLanguageCodeToInputMethodIdsMap_: function() {
      var inputMethodList = loadTimeData.getValue('inputMethodList');
      for (var i = 0; i < inputMethodList.length; i++) {
        var inputMethod = inputMethodList[i];
        for (var languageCode in inputMethod.languageCodeSet) {
          if (languageCode in this.languageCodeToInputMethodIdsMap_) {
            this.languageCodeToInputMethodIdsMap_[languageCode].push(
                inputMethod.id);
          } else {
            this.languageCodeToInputMethodIdsMap_[languageCode] =
                [inputMethod.id];
          }
        }
      }
    },

    /**
     * Updates the currently selected language name.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateSelectedLanguageName_: function(languageCode) {
      var languageInfo = LanguageList.getLanguageInfoFromLanguageCode(
          languageCode);
      var languageDisplayName = languageInfo.displayName;
      var languageNativeDisplayName = languageInfo.nativeDisplayName;
      var textDirection = languageInfo.textDirection;

      // If the native name is different, add it.
      if (languageDisplayName != languageNativeDisplayName) {
        languageDisplayName += ' - ' + languageNativeDisplayName;
      }

      // Update the currently selected language name.
      var languageName = $('language-options-language-name');
      languageName.textContent = languageDisplayName;
      languageName.dir = textDirection;
    },

    /**
     * Updates the UI language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateUiLanguageButton_: function(languageCode) {
      var uiLanguageButton = $('language-options-ui-language-button');
      var uiLanguageMessage = $('language-options-ui-language-message');
      var uiLanguageNotification = $('language-options-ui-notification-bar');

      // Remove the event listener and add it back if useful.
      uiLanguageButton.onclick = null;

      // Unhide the language button every time, as it could've been previously
      // hidden by a language change.
      uiLanguageButton.hidden = false;

      // Hide the controlled setting indicator.
      var uiLanguageIndicator = document.querySelector(
          '.language-options-contents .controlled-setting-indicator');
      uiLanguageIndicator.removeAttribute('controlled-by');

      if (languageCode == this.prospectiveUiLanguageCode_) {
        uiLanguageMessage.textContent =
            loadTimeData.getString('isDisplayedInThisLanguage');
        showMutuallyExclusiveNodes(
            [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 1);
      } else if (languageCode in loadTimeData.getValue('uiLanguageCodeSet')) {
        if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest()) {
          // In the guest mode for ChromeOS, changing UI language does not make
          // sense because it does not take effect after browser restart.
          uiLanguageButton.hidden = true;
          uiLanguageMessage.hidden = true;
        } else {
          uiLanguageButton.textContent =
              loadTimeData.getString('displayInThisLanguage');

          if (loadTimeData.valueExists('secondaryUser') &&
              loadTimeData.getBoolean('secondaryUser')) {
            uiLanguageButton.disabled = true;
            uiLanguageIndicator.setAttribute('controlled-by', 'shared');
          } else {
            uiLanguageButton.onclick = function(e) {
              chrome.send('uiLanguageChange', [languageCode]);
            };
          }
          showMutuallyExclusiveNodes(
              [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 0);
        }
      } else {
        uiLanguageMessage.textContent =
            loadTimeData.getString('cannotBeDisplayedInThisLanguage');
        showMutuallyExclusiveNodes(
            [uiLanguageButton, uiLanguageMessage, uiLanguageNotification], 1);
      }
    },

    /**
     * Updates the spell check language button.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateSpellCheckLanguageButton_: function(languageCode) {
      var spellCheckLanguageSection = $('language-options-spellcheck');
      var spellCheckLanguageButton =
          $('language-options-spell-check-language-button');
      var spellCheckLanguageMessage =
          $('language-options-spell-check-language-message');
      var dictionaryDownloadInProgress =
          $('language-options-dictionary-downloading-message');
      var dictionaryDownloadFailed =
          $('language-options-dictionary-download-failed-message');
      var dictionaryDownloadFailHelp =
          $('language-options-dictionary-download-fail-help-message');
      spellCheckLanguageSection.hidden = false;
      spellCheckLanguageMessage.hidden = true;
      spellCheckLanguageButton.hidden = true;
      dictionaryDownloadInProgress.hidden = true;
      dictionaryDownloadFailed.hidden = true;
      dictionaryDownloadFailHelp.hidden = true;

      if (languageCode == this.spellCheckDictionary_) {
        if (!(languageCode in this.spellcheckDictionaryDownloadStatus_)) {
          spellCheckLanguageMessage.textContent =
              loadTimeData.getString('isUsedForSpellChecking');
          showMutuallyExclusiveNodes(
              [spellCheckLanguageButton, spellCheckLanguageMessage], 1);
        } else if (this.spellcheckDictionaryDownloadStatus_[languageCode] ==
                       DOWNLOAD_STATUS.IN_PROGRESS) {
          dictionaryDownloadInProgress.hidden = false;
        } else if (this.spellcheckDictionaryDownloadStatus_[languageCode] ==
                       DOWNLOAD_STATUS.FAILED) {
          spellCheckLanguageSection.hidden = true;
          dictionaryDownloadFailed.hidden = false;
          if (this.spellcheckDictionaryDownloadFailures_ > 1)
            dictionaryDownloadFailHelp.hidden = false;
        }
      } else if (languageCode in
          loadTimeData.getValue('spellCheckLanguageCodeSet')) {
        spellCheckLanguageButton.textContent =
            loadTimeData.getString('useThisForSpellChecking');
        showMutuallyExclusiveNodes(
            [spellCheckLanguageButton, spellCheckLanguageMessage], 0);
        spellCheckLanguageButton.languageCode = languageCode;
      } else if (!languageCode) {
        spellCheckLanguageButton.hidden = true;
        spellCheckLanguageMessage.hidden = true;
      } else {
        spellCheckLanguageMessage.textContent =
            loadTimeData.getString('cannotBeUsedForSpellChecking');
        showMutuallyExclusiveNodes(
            [spellCheckLanguageButton, spellCheckLanguageMessage], 1);
      }
    },

    /**
     * Updates the checkbox for stopping translation.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateOfferToTranslateCheckbox_: function(languageCode) {
      var div = $('language-options-offer-to-translate');

      // Translation server supports Chinese (Transitional) and Chinese
      // (Simplified) but not 'general' Chinese. To avoid ambiguity, we don't
      // show this preference when general Chinese is selected.
      if (languageCode != 'zh') {
        div.hidden = false;
      } else {
        div.hidden = true;
        return;
      }

      var offerToTranslate = div.querySelector('div');
      var cannotTranslate = $('cannot-translate-in-this-language');
      var nodes = [offerToTranslate, cannotTranslate];

      var convertedLangCode = this.convertLangCodeForTranslation_(languageCode);
      if (this.translateSupportedLanguages_.indexOf(convertedLangCode) != -1) {
        showMutuallyExclusiveNodes(nodes, 0);
      } else {
        showMutuallyExclusiveNodes(nodes, 1);
        return;
      }

      var checkbox = $('offer-to-translate-in-this-language');

      if (!this.enableTranslate_) {
        checkbox.disabled = true;
        checkbox.checked = false;
        return;
      }

      // If the language corresponds to the default target language (in most
      // cases, the user's locale language), "Offer to translate" checkbox
      // should be always unchecked.
      var defaultTargetLanguage =
          loadTimeData.getString('defaultTargetLanguage');
      if (convertedLangCode == defaultTargetLanguage) {
        checkbox.disabled = true;
        checkbox.checked = false;
        return;
      }

      checkbox.disabled = false;

      var blockedLanguages = this.translateBlockedLanguages_;
      var checked = blockedLanguages.indexOf(convertedLangCode) == -1;
      checkbox.checked = checked;
    },

    /**
     * Updates the input method list.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateInputMethodList_: function(languageCode) {
      // Give one of the checkboxes or buttons focus, if it's specified in the
      // URL hash (ex. focus=mozc). Used for automated testing.
      var focusInputMethodId = -1;
      var match = document.location.hash.match(/\bfocus=([\w:-]+)\b/);
      if (match) {
        focusInputMethodId = match[1];
      }
      // Change the visibility of the input method list. Input methods that
      // matches |languageCode| will become visible.
      var inputMethodList = $('language-options-input-method-list');
      var methods = inputMethodList.querySelectorAll('.input-method');
      for (var i = 0; i < methods.length; i++) {
        var method = methods[i];
        if (languageCode in method.languageCodeSet) {
          method.hidden = false;
          var input = method.querySelector('input');
          // Give it focus if the ID matches.
          if (input.inputMethodId == focusInputMethodId) {
            input.focus();
          }
        } else {
          method.hidden = true;
        }
      }

      $('language-options-input-method-none').hidden =
          (languageCode in this.languageCodeToInputMethodIdsMap_);

      if (focusInputMethodId == 'add') {
        $('language-options-add-button').focus();
      }
    },

    /**
     * Updates the language list in the add language overlay.
     * @param {string} languageCode Language code (ex. "fr").
     * @private
     */
    updateLanguageListInAddLanguageOverlay_: function(languageCode) {
      // Change the visibility of the language list in the add language
      // overlay. Languages that are already active will become invisible,
      // so that users don't add the same language twice.
      var languageOptionsList = $('language-options-list');
      var languageCodes = languageOptionsList.getLanguageCodes();
      var languageCodeSet = {};
      for (var i = 0; i < languageCodes.length; i++) {
        languageCodeSet[languageCodes[i]] = true;
      }

      var addLanguageList = $('add-language-overlay-language-list');
      var options = addLanguageList.querySelectorAll('option');
      assert(options.length > 0);
      var selectedFirstItem = false;
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        option.hidden = option.value in languageCodeSet;
        if (!option.hidden && !selectedFirstItem) {
          // Select first visible item, otherwise previously selected hidden
          // item will be selected by default at the next time.
          option.selected = true;
          selectedFirstItem = true;
        }
      }
    },

    /**
     * Handles preloadEnginesPref change.
     * @param {Event} e Change event.
     * @private
     */
    handlePreloadEnginesPrefChange_: function(e) {
      var value = e.value.value;
      this.preloadEngines_ = this.filterBadPreloadEngines_(value.split(','));
      this.updateCheckboxesFromPreloadEngines_();
      $('language-options-list').updateDeletable();
    },

    /**
     * Handles enabledExtensionImePref change.
     * @param {Event} e Change event.
     * @private
     */
    handleEnabledExtensionsPrefChange_: function(e) {
      var value = e.value.value;
      this.enabledExtensionImes_ = value.split(',');
      this.updateCheckboxesFromEnabledExtensions_();
    },

    /**
     * Handles offer-to-translate checkbox's click event.
     * @param {Event} e Click event.
     * @private
     */
    handleOfferToTranslateCheckboxClick_: function(e) {
      var checkbox = e.target;
      var checked = checkbox.checked;

      var languageOptionsList = $('language-options-list');
      var selectedLanguageCode = languageOptionsList.getSelectedLanguageCode();

      if (checked)
        this.removeBlockedLanguage_(selectedLanguageCode);
      else
        this.addBlockedLanguage_(selectedLanguageCode);
    },

    /**
     * Handles input method checkbox's click event.
     * @param {Event} e Click event.
     * @private
     */
    handleCheckboxClick_: function(e) {
      var checkbox = e.target;

      // Third party IMEs require additional confirmation prior to enabling due
      // to privacy risk.
      if (/^_ext_ime_/.test(checkbox.inputMethodId) && checkbox.checked) {
        var confirmationCallback = this.handleCheckboxUpdate_.bind(this,
                                                                   checkbox);
        var cancellationCallback = function() {
          checkbox.checked = false;
        };
        ThirdPartyImeConfirmOverlay.showConfirmationDialog({
          extension: checkbox.imeProvider,
          confirm: confirmationCallback,
          cancel: cancellationCallback
        });
      } else {
        this.handleCheckboxUpdate_(checkbox);
      }
    },

    /**
     * Updates active IMEs based on change in state of a checkbox for an input
     * method.
     * @param {!Element} checkbox Updated checkbox element.
     * @private
     */
    handleCheckboxUpdate_: function(checkbox) {
      if (checkbox.inputMethodId.match(/^_ext_ime_/)) {
        this.updateEnabledExtensionsFromCheckboxes_();
        this.saveEnabledExtensionPref_();
        return;
      }
      if (this.preloadEngines_.length == 1 && !checkbox.checked) {
        // Don't allow disabling the last input method.
        this.showNotification_(
            loadTimeData.getString('pleaseAddAnotherInputMethod'),
            loadTimeData.getString('okButton'));
        checkbox.checked = true;
        return;
      }
      if (checkbox.checked) {
        chrome.send('inputMethodEnable', [checkbox.inputMethodId]);
      } else {
        chrome.send('inputMethodDisable', [checkbox.inputMethodId]);
      }
      this.updatePreloadEnginesFromCheckboxes_();
      this.preloadEngines_ = this.sortPreloadEngines_(this.preloadEngines_);
      this.savePreloadEnginesPref_();
    },

    /**
     * Handles clicks on the "OK" button of the "Add language" dialog.
     * @param {Event} e Click event.
     * @private
     */
    handleAddLanguageOkButtonClick_: function(e) {
      var languagesSelect = $('add-language-overlay-language-list');
      var selectedIndex = languagesSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = languagesSelect.options[selectedIndex];
        var langCode = String(selection.value);
        $('language-options-list').addLanguage(langCode);
        this.addBlockedLanguage_(langCode);
        OptionsPage.closeOverlay();
      }
    },

    /**
     * Checks if languageCode is deletable or not.
     * @param {string} languageCode the languageCode to check for deletability.
     */
    languageIsDeletable: function(languageCode) {
      // Don't allow removing the language if it's a UI language.
      if (languageCode == this.prospectiveUiLanguageCode_)
        return false;
      return (!cr.isChromeOS ||
              this.canDeleteLanguage_(languageCode));
    },

    /**
     * Handles browse.enable_spellchecking change.
     * @param {Event} e Change event.
     * @private
     */
    updateEnableSpellCheck_: function() {
       var value = !$('enable-spell-check').checked;
       $('language-options-spell-check-language-button').disabled = value;
       if (!cr.IsMac)
         $('edit-dictionary-button').hidden = value;
     },

    /**
     * Handles translateBlockedLanguagesPref change.
     * @param {Event} e Change event.
     * @private
     */
    handleTranslateBlockedLanguagesPrefChange_: function(e) {
      this.translateBlockedLanguages_ = e.value.value;
      this.updateOfferToTranslateCheckbox_(
          $('language-options-list').getSelectedLanguageCode());
    },

    /**
     * Handles spellCheckDictionaryPref change.
     * @param {Event} e Change event.
     * @private
     */
    handleSpellCheckDictionaryPrefChange_: function(e) {
      var languageCode = e.value.value;
      this.spellCheckDictionary_ = languageCode;
      if (!cr.isMac) {
        this.updateSpellCheckLanguageButton_(
            $('language-options-list').getSelectedLanguageCode());
      }
    },

    /**
     * Handles translate.enabled change.
     * @param {Event} e Change event.
     * @private
     */
    handleEnableTranslatePrefChange_: function(e) {
      var enabled = e.value.value;
      this.enableTranslate_ = enabled;
      this.updateOfferToTranslateCheckbox_(
          $('language-options-list').getSelectedLanguageCode());
    },

    /**
     * Handles spellCheckLanguageButton click.
     * @param {Event} e Click event.
     * @private
     */
    handleSpellCheckLanguageButtonClick_: function(e) {
      var languageCode = e.target.languageCode;
      // Save the preference.
      Preferences.setStringPref(SPELL_CHECK_DICTIONARY_PREF,
                                languageCode, true);
      chrome.send('spellCheckLanguageChange', [languageCode]);
    },

    /**
     * Checks whether it's possible to remove the language specified by
     * languageCode and returns true if possible. This function returns false
     * if the removal causes the number of preload engines to be zero.
     *
     * @param {string} languageCode Language code (ex. "fr").
     * @return {boolean} Returns true on success.
     * @private
     */
    canDeleteLanguage_: function(languageCode) {
      // First create the set of engines to be removed from input methods
      // associated with the language code.
      var enginesToBeRemovedSet = {};
      var inputMethodIds = this.languageCodeToInputMethodIdsMap_[languageCode];

      // If this language doesn't have any input methods, it can be deleted.
      if (!inputMethodIds)
        return true;

      for (var i = 0; i < inputMethodIds.length; i++) {
        enginesToBeRemovedSet[inputMethodIds[i]] = true;
      }

      // Then eliminate engines that are also used for other active languages.
      // For instance, if "xkb:us::eng" is used for both English and Filipino.
      var languageCodes = $('language-options-list').getLanguageCodes();
      for (var i = 0; i < languageCodes.length; i++) {
        // Skip the target language code.
        if (languageCodes[i] == languageCode) {
          continue;
        }
        // Check if input methods used in this language are included in
        // enginesToBeRemovedSet. If so, eliminate these from the set, so
        // we don't remove this time.
        var inputMethodIdsForAnotherLanguage =
            this.languageCodeToInputMethodIdsMap_[languageCodes[i]];
        if (!inputMethodIdsForAnotherLanguage)
          continue;

        for (var j = 0; j < inputMethodIdsForAnotherLanguage.length; j++) {
          var inputMethodId = inputMethodIdsForAnotherLanguage[j];
          if (inputMethodId in enginesToBeRemovedSet) {
            delete enginesToBeRemovedSet[inputMethodId];
          }
        }
      }

      // Update the preload engine list with the to-be-removed set.
      var newPreloadEngines = [];
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        if (!(this.preloadEngines_[i] in enginesToBeRemovedSet)) {
          newPreloadEngines.push(this.preloadEngines_[i]);
        }
      }
      // Don't allow this operation if it causes the number of preload
      // engines to be zero.
      return (newPreloadEngines.length > 0);
    },

    /**
     * Saves the enabled extension preference.
     * @private
     */
    saveEnabledExtensionPref_: function() {
      Preferences.setStringPref(ENABLED_EXTENSION_IME_PREF,
                                this.enabledExtensionImes_.join(','), true);
    },

    /**
     * Updates the checkboxes in the input method list from the enabled
     * extensions preference.
     * @private
     */
    updateCheckboxesFromEnabledExtensions_: function() {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      for (var i = 0; i < this.enabledExtensionImes_.length; i++)
        dictionary[this.enabledExtensionImes_[i]] = true;

      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].inputMethodId.match(/^_ext_ime_/))
          checkboxes[i].checked = (checkboxes[i].inputMethodId in dictionary);
      }
      var configureButtons = inputMethodList.querySelectorAll('button');
      for (var i = 0; i < configureButtons.length; i++) {
        if (configureButtons[i].inputMethodId.match(/^_ext_ime_/)) {
          configureButtons[i].hidden =
              !(configureButtons[i].inputMethodId in dictionary);
        }
      }
    },

    /**
     * Updates the enabled extensions preference from the checkboxes in the
     * input method list.
     * @private
     */
    updateEnabledExtensionsFromCheckboxes_: function() {
      this.enabledExtensionImes_ = [];
      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].inputMethodId.match(/^_ext_ime_/)) {
          if (checkboxes[i].checked)
            this.enabledExtensionImes_.push(checkboxes[i].inputMethodId);
        }
      }
    },

    /**
     * Saves the preload engines preference.
     * @private
     */
    savePreloadEnginesPref_: function() {
      Preferences.setStringPref(PRELOAD_ENGINES_PREF,
                                this.preloadEngines_.join(','), true);
    },

    /**
     * Updates the checkboxes in the input method list from the preload
     * engines preference.
     * @private
     */
    updateCheckboxesFromPreloadEngines_: function() {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      for (var i = 0; i < this.preloadEngines_.length; i++) {
        dictionary[this.preloadEngines_[i]] = true;
      }

      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (!checkboxes[i].inputMethodId.match(/^_ext_ime_/))
          checkboxes[i].checked = (checkboxes[i].inputMethodId in dictionary);
      }
      var configureButtons = inputMethodList.querySelectorAll('button');
      for (var i = 0; i < configureButtons.length; i++) {
        if (!configureButtons[i].inputMethodId.match(/^_ext_ime_/)) {
          configureButtons[i].hidden =
              !(configureButtons[i].inputMethodId in dictionary);
        }
      }
    },

    /**
     * Updates the preload engines preference from the checkboxes in the
     * input method list.
     * @private
     */
    updatePreloadEnginesFromCheckboxes_: function() {
      this.preloadEngines_ = [];
      var inputMethodList = $('language-options-input-method-list');
      var checkboxes = inputMethodList.querySelectorAll('input');
      for (var i = 0; i < checkboxes.length; i++) {
        if (!checkboxes[i].inputMethodId.match(/^_ext_ime_/)) {
          if (checkboxes[i].checked)
            this.preloadEngines_.push(checkboxes[i].inputMethodId);
        }
      }
      var languageOptionsList = $('language-options-list');
      languageOptionsList.updateDeletable();
    },

    /**
     * Filters bad preload engines in case bad preload engines are
     * stored in the preference. Removes duplicates as well.
     * @param {Array} preloadEngines List of preload engines.
     * @private
     */
    filterBadPreloadEngines_: function(preloadEngines) {
      // Convert the list into a dictonary for simpler lookup.
      var dictionary = {};
      var list = loadTimeData.getValue('inputMethodList');
      for (var i = 0; i < list.length; i++) {
        dictionary[list[i].id] = true;
      }

      var enabledPreloadEngines = [];
      var seen = {};
      for (var i = 0; i < preloadEngines.length; i++) {
        // Check if the preload engine is present in the
        // dictionary, and not duplicate. Otherwise, skip it.
        // Component Extension IME should be handled same as preloadEngines and
        // "_comp_" is the special prefix of its ID.
        if ((preloadEngines[i] in dictionary && !(preloadEngines[i] in seen)) ||
            /^_comp_/.test(preloadEngines[i])) {
          enabledPreloadEngines.push(preloadEngines[i]);
          seen[preloadEngines[i]] = true;
        }
      }
      return enabledPreloadEngines;
    },

    // TODO(kochi): This is an adapted copy from new_tab.js.
    // If this will go as final UI, refactor this to share the component with
    // new new tab page.
    /**
     * Shows notification
     * @private
     */
    notificationTimeout_: null,
    showNotification_: function(text, actionText, opt_delay) {
      var notificationElement = $('notification');
      var actionLink = notificationElement.querySelector('.link-color');
      var delay = opt_delay || 10000;

      function show() {
        window.clearTimeout(this.notificationTimeout_);
        notificationElement.classList.add('show');
        document.body.classList.add('notification-shown');
      }

      function hide() {
        window.clearTimeout(this.notificationTimeout_);
        notificationElement.classList.remove('show');
        document.body.classList.remove('notification-shown');
        // Prevent tabbing to the hidden link.
        actionLink.tabIndex = -1;
        // Setting tabIndex to -1 only prevents future tabbing to it. If,
        // however, the user switches window or a tab and then moves back to
        // this tab the element may gain focus. We therefore make sure that we
        // blur the element so that the element focus is not restored when
        // coming back to this window.
        actionLink.blur();
      }

      function delayedHide() {
        this.notificationTimeout_ = window.setTimeout(hide, delay);
      }

      notificationElement.firstElementChild.textContent = text;
      actionLink.textContent = actionText;

      actionLink.onclick = hide;
      actionLink.onkeydown = function(e) {
        if (e.keyIdentifier == 'Enter') {
          hide();
        }
      };
      notificationElement.onmouseover = show;
      notificationElement.onmouseout = delayedHide;
      actionLink.onfocus = show;
      actionLink.onblur = delayedHide;
      // Enable tabbing to the link now that it is shown.
      actionLink.tabIndex = 0;

      show();
      delayedHide();
    },

    /**
     * Chrome callback for when the UI language preference is saved.
     * @param {string} languageCode The newly selected language to use.
     * @private
     */
    uiLanguageSaved_: function(languageCode) {
      this.prospectiveUiLanguageCode_ = languageCode;

      // If the user is no longer on the same language code, ignore.
      if ($('language-options-list').getSelectedLanguageCode() != languageCode)
        return;

      // Special case for when a user changes to a different language, and
      // changes back to the same language without having restarted Chrome or
      // logged in/out of ChromeOS.
      if (languageCode == loadTimeData.getString('currentUiLanguageCode')) {
        this.updateUiLanguageButton_(languageCode);
        return;
      }

      // Otherwise, show a notification telling the user that their changes will
      // only take effect after restart.
      showMutuallyExclusiveNodes([$('language-options-ui-language-button'),
                                  $('language-options-ui-notification-bar')],
                                 1);
    },

    /**
     * A handler for when dictionary for |languageCode| begins downloading.
     * @param {string} languageCode The language of the dictionary that just
     *     began downloading.
     * @private
     */
    onDictionaryDownloadBegin_: function(languageCode) {
      this.spellcheckDictionaryDownloadStatus_[languageCode] =
          DOWNLOAD_STATUS.IN_PROGRESS;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    },

    /**
     * A handler for when dictionary for |languageCode| successfully downloaded.
     * @param {string} languageCode The language of the dictionary that
     *     succeeded downloading.
     * @private
     */
    onDictionaryDownloadSuccess_: function(languageCode) {
      delete this.spellcheckDictionaryDownloadStatus_[languageCode];
      this.spellcheckDictionaryDownloadFailures_ = 0;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    },

    /**
     * A handler for when dictionary for |languageCode| fails to download.
     * @param {string} languageCode The language of the dictionary that failed
     *     to download.
     * @private
     */
    onDictionaryDownloadFailure_: function(languageCode) {
      this.spellcheckDictionaryDownloadStatus_[languageCode] =
          DOWNLOAD_STATUS.FAILED;
      this.spellcheckDictionaryDownloadFailures_++;
      if (!cr.isMac &&
          languageCode ==
              $('language-options-list').getSelectedLanguageCode()) {
        this.updateSpellCheckLanguageButton_(languageCode);
      }
    },

    /*
     * Converts the language code for Translation. There are some differences
     * between the language set for Translation and that for Accept-Language.
     * @param {string} languageCode The language code like 'fr'.
     * @return {string} The converted language code.
     * @private
     */
    convertLangCodeForTranslation_: function(languageCode) {
      var tokens = languageCode.split('-');
      var main = tokens[0];

      // See also: chrome/renderer/translate/translate_helper.cc.
      var synonyms = {
        'nb': 'no',
        'he': 'iw',
        'jv': 'jw',
        'fil': 'tl',
      };

      if (main in synonyms) {
        return synonyms[main];
      } else if (main == 'zh') {
        // In Translation, general Chinese is not used, and the sub code is
        // necessary as a language code for Translate server.
        return languageCode;
      }

      return main;
    },
  };

  /**
   * Shows the node at |index| in |nodes|, hides all others.
   * @param {Array<HTMLElement>} nodes The nodes to be shown or hidden.
   * @param {number} index The index of |nodes| to show.
   */
  function showMutuallyExclusiveNodes(nodes, index) {
    assert(index >= 0 && index < nodes.length);
    for (var i = 0; i < nodes.length; ++i) {
      assert(nodes[i] instanceof HTMLElement);  // TODO(dbeam): Ignore null?
      nodes[i].hidden = i != index;
    }
  }

  LanguageOptions.uiLanguageSaved = function(languageCode) {
    LanguageOptions.getInstance().uiLanguageSaved_(languageCode);
  };

  LanguageOptions.onDictionaryDownloadBegin = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadBegin_(languageCode);
  };

  LanguageOptions.onDictionaryDownloadSuccess = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadSuccess_(languageCode);
  };

  LanguageOptions.onDictionaryDownloadFailure = function(languageCode) {
    LanguageOptions.getInstance().onDictionaryDownloadFailure_(languageCode);
  };

  LanguageOptions.onComponentManagerInitialized = function(componentImes) {
    LanguageOptions.getInstance().appendComponentExtensionIme_(componentImes);
  };

  // Export
  return {
    LanguageOptions: LanguageOptions
  };
});
