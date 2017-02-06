// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' provides convenient access to
 * Chrome's language and input method settings.
 *
 * Instances of this element have a 'languages' property, which reflects the
 * current language settings. The 'languages' property is read-only, meaning
 * hosts using this element cannot change it directly. Instead, changes to
 * language settings should be made using the LanguageHelperImpl singleton.
 *
 * Use upward binding syntax to propagate changes from child to host, so that
 * changes made internally to 'languages' propagate to your host element:
 *
 *     <template>
 *       <settings-languages languages="{{languages}}">
 *       </settings-languages>
 *       <div>[[languages.someProperty]]</div>
 *     </template>
 */

var SettingsLanguagesSingletonElement;

cr.exportPath('languageSettings');

(function() {
'use strict';

// Translate server treats some language codes the same.
// See also: components/translate/core/common/translate_util.cc.
var kLanguageCodeToTranslateCode = {
  'nb': 'no',
  'fil': 'tl',
  'zh-HK': 'zh-TW',
  'zh-MO': 'zh-TW',
  'zh-SG': 'zh-CN',
};

// Some ISO 639 language codes have been renamed, e.g. "he" to "iw", but
// Translate still uses the old versions. TODO(michaelpg): Chrome does too.
// Follow up with Translate owners to understand the right thing to do.
var kTranslateLanguageSynonyms = {
  'he': 'iw',
  'jv': 'jw',
};

var preferredLanguagesPrefName = cr.isChromeOS ?
    'settings.language.preferred_languages' : 'intl.accept_languages';

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change. These
 * updates propagate to each <settings-language> instance so that their
 * 'languages' property updates like any other Polymer property.
 * @implements {LanguageHelper}
 */
SettingsLanguagesSingletonElement = Polymer({
  is: 'settings-languages-singleton',

  behaviors: [PrefsBehavior],

  properties: {
    /**
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /**
     * Object containing all preferences.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * PromiseResolver to be resolved when the singleton has been initialized.
     * @private {!PromiseResolver}
     */
    resolver_: {
      type: Object,
      value: function() {
        return new PromiseResolver();
      },
    },

    /** @type {!LanguageSettingsPrivate} */
    languageSettingsPrivate: Object,

    /** @type {!InputMethodPrivate} */
    inputMethodPrivate: Object,
  },

  /**
   * Hash map of supported languages by language codes for fast lookup.
   * @private {!Map<string, !chrome.languageSettingsPrivate.Language>}
   */
  supportedLanguageMap_: new Map(),

  /**
   * Hash set of enabled language codes for membership testing.
   * @private {!Set<string>}
   */
  enabledLanguageSet_: new Set(),

  /**
   * Hash map of supported input methods by ID for fast lookup.
   * @private {!Map<string, chrome.languageSettingsPrivate.InputMethod>}
   */
  supportedInputMethodMap_: new Map(),

  /**
   * Hash map of input methods supported for each language.
   * @type {!Map<string, !Array<!chrome.languageSettingsPrivate.InputMethod>>}
   * @private
   */
  languageInputMethods_: new Map(),

  observers: [
    'preferredLanguagesPrefChanged_(' +
        'prefs.' + preferredLanguagesPrefName + '.value, languages)',
    'spellCheckDictionariesPrefChanged_(' +
        'prefs.spellcheck.dictionaries.value.*, languages)',
    'translateLanguagesPrefChanged_(' +
        'prefs.translate_blocked_languages.value.*, languages)',
    'prospectiveUILanguageChanged_(' +
        'prefs.intl.app_locale.value, languages)',
    // Observe Chrome OS prefs (ignored for non-Chrome OS).
    'updateRemovableLanguages_(' +
        'prefs.settings.language.preload_engines.value, ' +
        'prefs.settings.language.enabled_extension_imes.value, ' +
        'languages)',
  ],

  /** @override */
  created: function() {
    this.languageSettingsPrivate =
        languageSettings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */(chrome.languageSettingsPrivate);

    this.inputMethodPrivate =
        languageSettings.inputMethodPrivateApiForTest ||
        /** @type {!InputMethodPrivate} */(chrome.inputMethodPrivate);

    var promises = [];

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises[0] = CrSettingsPrefs.initialized;

    // Get the language list.
    promises[1] = new Promise(function(resolve) {
      this.languageSettingsPrivate.getLanguageList(resolve);
    }.bind(this));

    // Get the translate target language.
    promises[2] = new Promise(function(resolve) {
      this.languageSettingsPrivate.getTranslateTargetLanguage(resolve);
    }.bind(this));

    if (cr.isChromeOS) {
      promises[3] = new Promise(function(resolve) {
        this.languageSettingsPrivate.getInputMethodLists(function(lists) {
          resolve(lists.componentExtensionImes.concat(
              lists.thirdPartyExtensionImes));
        });
      }.bind(this));

      promises[4] = new Promise(function(resolve) {
        this.inputMethodPrivate.getCurrentInputMethod(resolve);
      }.bind(this));
    }

    Promise.all(promises).then(function(results) {
      this.createModel_(results[1], results[2], results[3], results[4]);
      this.resolver_.resolve();
    }.bind(this));

    if (cr.isChromeOS) {
      this.inputMethodPrivate.onChanged.addListener(
          this.onInputMethodChanged_.bind(this));
    }
  },

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   * @private
   */
  preferredLanguagesPrefChanged_: function() {
    var enabledLanguageStates =
        this.getEnabledLanguageStates_(this.languages.translateTarget);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (var languageState of enabledLanguageStates)
      this.enabledLanguageSet_.add(languageState.language.code);

    this.set('languages.enabled', enabledLanguageStates);
    this.updateRemovableLanguages_();
  },

  /**
   * Updates the spellCheckEnabled state of each enabled language.
   * @private
   */
  spellCheckDictionariesPrefChanged_: function() {
    var spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */(
        this.getPref('spellcheck.dictionaries').value));
    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set('languages.enabled.' + i + '.spellCheckEnabled',
               !!spellCheckSet.has(languageState.language.code));
    }
  },

  /** @private */
  translateLanguagesPrefChanged_: function() {
    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    for (var i = 0; i < this.languages.enabled.length; i++) {
      var translateCode = this.convertLanguageCodeForTranslate(
          this.languages.enabled[i].language.code);
      this.set(
          'languages.enabled.' + i + '.translateEnabled',
          !translateBlockedSet.has(translateCode));
    }
  },

  /** @private */
  prospectiveUILanguageChanged_: function() {
    this.updateRemovableLanguages_();
  },

  /**
   * Constructs the languages model.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @param {!Array<!chrome.languageSettingsPrivate.InputMethod>|undefined}
   *     supportedInputMethods Input methods (Chrome OS only).
   * @param {string|undefined} currentInputMethodId ID of the currently used
   *     input method (Chrome OS only).
   * @private
   */
  createModel_: function(supportedLanguages, translateTarget,
                         supportedInputMethods, currentInputMethodId) {
    // Populate the hash map of supported languages.
    for (var language of supportedLanguages) {
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      this.supportedLanguageMap_.set(language.code, language);
    }

    if (supportedInputMethods) {
      // Populate the hash map of supported input methods.
      for (var inputMethod of supportedInputMethods) {
        inputMethod.enabled = !!inputMethod.enabled;
        // Add the input method to the map of IDs.
        this.supportedInputMethodMap_.set(inputMethod.id, inputMethod);
        // Add the input method to the list of input methods for each language
        // it supports.
        for (var languageCode of inputMethod.languageCodes) {
          if (!this.supportedLanguageMap_.has(languageCode))
            continue;
          if (!this.languageInputMethods_.has(languageCode))
            this.languageInputMethods_.set(languageCode, [inputMethod]);
          else
            this.languageInputMethods_.get(languageCode).push(inputMethod);
        }
      }
    }

    // Create a list of enabled languages from the supported languages.
    var enabledLanguageStates = this.getEnabledLanguageStates_(translateTarget);
    // Populate the hash set of enabled languages.
    for (var languageState of enabledLanguageStates)
      this.enabledLanguageSet_.add(languageState.language.code);

    var model = /** @type {!LanguagesModel} */({
      supported: supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: translateTarget,
    });
    if (cr.isChromeOS) {
      model.inputMethods = /** @type {!InputMethodsModel} */({
        supported: supportedInputMethods,
        enabled: this.getEnabledInputMethods_(),
        currentId: currentInputMethodId,
      });
    }

    // Initialize the Polymer languages model.
    this.languages = model;
  },

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param {string} translateTarget Language code of the default translate
   *     target language.
   * @return {!Array<!LanguageState>}
   * @private
   */
  getEnabledLanguageStates_: function(translateTarget) {
    assert(CrSettingsPrefs.isInitialized);

    var pref = this.getPref(preferredLanguagesPrefName);
    var enabledLanguageCodes = pref.value.split(',');
    var spellCheckPref = this.getPref('spellcheck.dictionaries');
    var spellCheckSet = this.makeSetFromArray_(/** @type {!Array<string>} */(
        spellCheckPref.value));

    var translateBlockedPref = this.getPref('translate_blocked_languages');
    var translateBlockedSet = this.makeSetFromArray_(
        /** @type {!Array<string>} */(translateBlockedPref.value));

    var enabledLanguageStates = [];
    for (var i = 0; i < enabledLanguageCodes.length; i++) {
      var code = enabledLanguageCodes[i];
      var language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language)
        continue;
      var languageState = /** @type {LanguageState} */({});
      languageState.language = language;
      languageState.spellCheckEnabled = !!spellCheckSet.has(code);
      // Translate is considered disabled if this language maps to any translate
      // language that is blocked.
      var translateCode = this.convertLanguageCodeForTranslate(code);
      languageState.translateEnabled = !!language.supportsTranslate &&
          !translateBlockedSet.has(translateCode) &&
          translateCode != translateTarget;
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  },

  /**
   * Returns a list of enabled input methods.
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   * @private
   */
  getEnabledInputMethods_: function() {
    assert(cr.isChromeOS);
    assert(CrSettingsPrefs.isInitialized);

    var enabledInputMethodIds =
        this.getPref('settings.language.preload_engines').value.split(',');
    enabledInputMethodIds = enabledInputMethodIds.concat(this.getPref(
        'settings.language.enabled_extension_imes').value.split(','));

    // Return only supported input methods.
    return enabledInputMethodIds.map(function(id) {
      return this.supportedInputMethodMap_.get(id);
    }.bind(this)).filter(function(inputMethod) {
      return !!inputMethod;
    });
  },

  /** @private */
  updateEnabledInputMethods_: function() {
    assert(cr.isChromeOS);
    var enabledInputMethods = this.getEnabledInputMethods_();
    var enabledInputMethodSet = this.makeSetFromArray_(enabledInputMethods);

    for (var i = 0; i < this.languages.inputMethods.supported.length; i++) {
      this.set('languages.inputMethods.supported.' + i + '.enabled',
          enabledInputMethodSet.has(this.languages.inputMethods.supported[i]));
    }
    this.set('languages.inputMethods.enabled', enabledInputMethods);
  },

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   * @private
   */
  updateRemovableLanguages_: function() {
    assert(this.languages);
    // TODO(michaelpg): Enabled input methods can affect which languages are
    // removable, so run updateEnabledInputMethods_ first (if it has been
    // scheduled).
    if (cr.isChromeOS)
      this.updateEnabledInputMethods_();

    for (var i = 0; i < this.languages.enabled.length; i++) {
      var languageState = this.languages.enabled[i];
      this.set('languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState.language.code));
    }
  },

  /**
   * Creates a Set from the elements of the array.
   * @param {!Array<T>} list
   * @return {!Set<T>}
   * @template T
   * @private
   */
  makeSetFromArray_: function(list) {
    var set = new Set();
    for (var item of list)
      set.add(item);
    return set;
  },

  // LanguageHelper implementation.
  // TODO(michaelpg): replace duplicate docs with @override once b/24294625
  // is fixed.

  /** @return {!Promise} */
  whenReady: function() {
    return this.resolver_.promise;
  },

  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: function(languageCode) {
    assert(cr.isChromeOS || cr.isWindows);
    chrome.send('setUILanguage', [languageCode]);
  },

  /** Resets the prospective UI language back to the actual UI language. */
  resetUILanguage: function() {
    assert(cr.isChromeOS || cr.isWindows);
    chrome.send('setUILanguage', [navigator.language]);
  },

  /**
   * Returns the "prospective" UI language, i.e. the one to be used on next
   * restart. If the pref is not set, the current UI language is also the
   * "prospective" language.
   * @return {string} Language code of the prospective UI language.
   */
  getProspectiveUILanguage: function() {
    return /** @type {string} */(this.getPref('intl.app_locale').value) ||
        navigator.language;
  },

  /**
   * @param {string} languageCode
   * @return {boolean} True if the language is enabled.
   */
  isLanguageEnabled: function(languageCode) {
    return this.enabledLanguageSet_.has(languageCode);
  },

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    if (languageCodes.indexOf(languageCode) > -1)
      return;
    languageCodes.push(languageCode);
    this.languageSettingsPrivate.setLanguageList(languageCodes);
    this.disableTranslateLanguage(languageCode);
  },

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: function(languageCode) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    assert(this.canDisableLanguage(languageCode));

    // Remove the language from spell check.
    this.deletePrefListItem('spellcheck.dictionaries', languageCode);

    if (cr.isChromeOS) {
      var inputMethods = this.languageInputMethods_.get(languageCode) || [];
      for (var inputMethod of inputMethods) {
        var supportsOtherEnabledLanguages = inputMethod.languageCodes.some(
            function(inputMethodLanguageCode) {
              return inputMethodLanguageCode != languageCode &&
                  this.isLanguageEnabled(languageCode);
            }.bind(this));
        if (!supportsOtherEnabledLanguages)
          this.removeInputMethod(inputMethod.id);
      }
    }

    // Remove the language from preferred languages.
    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');
    var languageIndex = languageCodes.indexOf(languageCode);
    if (languageIndex == -1)
      return;
    languageCodes.splice(languageIndex, 1);
    this.languageSettingsPrivate.setLanguageList(languageCodes);
    this.enableTranslateLanguage(languageCode);
  },

  /**
   * @param {string} languageCode Language code for an enabled language.
   * @return {boolean}
   */
  canDisableLanguage: function(languageCode) {
    // Cannot disable the prospective UI language.
    if ((cr.isChromeOS || cr.isWindows) &&
        languageCode == this.getProspectiveUILanguage()) {
      return false;
    }

    // Cannot disable the only enabled language.
    if (this.languages.enabled.length == 1)
      return false;

    if (!cr.isChromeOS)
      return true;

    // If this is the only enabled language that is supported by all enabled
    // component IMEs, it cannot be disabled because we need those IMEs.
    var otherInputMethodsEnabled = this.languages.enabled.some(
        function(languageState) {
          var otherLanguageCode = languageState.language.code;
          if (otherLanguageCode == languageCode)
            return false;
          var inputMethods = this.languageInputMethods_.get(otherLanguageCode);
          return inputMethods && inputMethods.some(function(inputMethod) {
            return this.isComponentIme(inputMethod) &&
                this.supportedInputMethodMap_.get(inputMethod.id).enabled;
          }, this);
        }, this);
    return otherInputMethodsEnabled;
  },

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param {string} languageCode
   * @param {number} offset Negative offset moves the language toward the front
   *     of the list. A Positive one moves the language toward the back.
   */
  moveLanguage: function(languageCode, offset) {
    if (!CrSettingsPrefs.isInitialized)
      return;

    var languageCodes =
        this.getPref(preferredLanguagesPrefName).value.split(',');

    var originalIndex = languageCodes.indexOf(languageCode);
    var newIndex = originalIndex + offset;
    if (originalIndex == -1 || newIndex < 0 || newIndex >= languageCodes.length)
      return;

    languageCodes.splice(originalIndex, 1);
    languageCodes.splice(newIndex, 0, languageCode);
    this.languageSettingsPrivate.setLanguageList(languageCodes);
  },

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: function(languageCode) {
    languageCode = this.convertLanguageCodeForTranslate(languageCode);
    this.deletePrefListItem('translate_blocked_languages', languageCode);
  },

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: function(languageCode) {
    this.appendPrefListItem('translate_blocked_languages',
        this.convertLanguageCodeForTranslate(languageCode));
  },

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: function(languageCode, enable) {
    if (!this.languages)
      return;

    if (enable) {
      var spellCheckPref = this.getPref('spellcheck.dictionaries');
      this.appendPrefListItem('spellcheck.dictionaries', languageCode);
    } else {
      this.deletePrefListItem('spellcheck.dictionaries', languageCode);
    }
  },

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   */
  convertLanguageCodeForTranslate: function(languageCode) {
    if (languageCode in kLanguageCodeToTranslateCode)
      return kLanguageCodeToTranslateCode[languageCode];

    var main = languageCode.split('-')[0];
    if (main == 'zh') {
      // In Translate, general Chinese is not used, and the sub code is
      // necessary as a language code for the Translate server.
      return languageCode;
    }
    if (main in kTranslateLanguageSynonyms)
      return kTranslateLanguageSynonyms[main];

    return main;
  },

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   * @param {string} languageCode
   * @return {string}
   */
  getLanguageCodeWithoutRegion: function(languageCode) {
    // The Norwegian languages fall under the 'no' macrolanguage.
    if (languageCode == 'nb' || languageCode == 'nn')
      return 'no';

    // Match the characters before the hyphen.
    var result = languageCode.match(/^([^-]+)-?/);
    assert(result.length == 2);
    return result[1];
  },

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage: function(languageCode) {
    return this.supportedLanguageMap_.get(languageCode);
  },

  /**
   * @param {string} id
   * @return {!chrome.languageSettingsPrivate.InputMethod|undefined}
   */
  getInputMethod: function(id) {
    assert(cr.isChromeOS);
    return this.supportedInputMethodMap_.get(id);
  },

  /** @param {string} id */
  addInputMethod: function(id) {
    assert(cr.isChromeOS);
    if (!this.supportedInputMethodMap_.has(id))
      return;
    this.languageSettingsPrivate.addInputMethod(id);
  },

  /** @param {string} id */
  removeInputMethod: function(id) {
    assert(cr.isChromeOS);
    if (!this.supportedInputMethodMap_.has(id))
      return;
    this.languageSettingsPrivate.removeInputMethod(id);
  },

  /** @param {string} id */
  setCurrentInputMethod: function(id) {
    assert(cr.isChromeOS);
    this.inputMethodPrivate.setCurrentInputMethod(id);
  },

  /**
   * param {string} languageCode
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguage: function(languageCode) {
    return this.languageInputMethods_.get(languageCode) || [];
  },

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @return {boolean}
   */
  isComponentIme: function(inputMethod) {
    assert(cr.isChromeOS);
    return inputMethod.id.startsWith('_comp_');
  },

  /** @param {string} id Input method ID. */
  openInputMethodOptions: function(id) {
    assert(cr.isChromeOS);
    this.inputMethodPrivate.openOptionsPage(id);
  },

  /** @param {string} id New current input method ID. */
  onInputMethodChanged_: function(id) {
    assert(cr.isChromeOS);
    this.set('languages.inputMethods.currentId', id);
  },

  /** @param {string} id Added input method ID. */
  onInputMethodAdded_: function(id) {
    assert(cr.isChromeOS);
    this.updateEnabledInputMethods_();
  },

  /** @param {string} id Removed input method ID. */
  onInputMethodRemoved_: function(id) {
    assert(cr.isChromeOS);
    this.updateEnabledInputMethods_();
  },
});
})();

/**
 * A reference to the singleton under the guise of a LanguageHelper
 * implementation. This provides a limited API but implies the singleton
 * should not be used directly for data binding.
 */
var LanguageHelperImpl = SettingsLanguagesSingletonElement;
cr.addSingletonGetter(LanguageHelperImpl);

/**
 * This element has a reference to the singleton, exposing the singleton's
 * |languages| model to the host of this element.
 */
Polymer({
  is: 'settings-languages',

  properties: {
    /**
     * A reference to the languages model from the singleton, exposed as a
     * read-only property so hosts can bind to it, but not change it.
     * @type {LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
      readOnly: true,
    },
  },

  ready: function() {
    var singleton = /** @type {!SettingsLanguagesSingletonElement} */
        (LanguageHelperImpl.getInstance());
    singleton.whenReady().then(function() {
      // Set the 'languages' property to reference the singleton's model.
      this._setLanguages(singleton.languages);
      // Listen for changes to the singleton's languages property, so we know
      // when to notify hosts of changes to (our reference to) the property.
      this.listen(singleton, 'languages-changed', 'singletonLanguagesChanged_');
    }.bind(this));
  },

  /**
   * Takes changes reported by the singleton and forwards them to the host,
   * manually sending a change notification for our 'languages' property (since
   * it's the same object as the singleton's property, but isn't bound by
   * Polymer).
   * @private
   */
  singletonLanguagesChanged_: function(e) {
    // Forward the change notification to the host.
    this.fire(e.type, e.detail, {bubbles: false});
  },
});
