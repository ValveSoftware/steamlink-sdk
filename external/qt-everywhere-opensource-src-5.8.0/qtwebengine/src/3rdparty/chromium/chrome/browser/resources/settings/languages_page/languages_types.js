// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Settings and state for a particular enabled language.
 * @typedef {{
 *   language: !chrome.languageSettingsPrivate.Language,
 *   removable: boolean,
 *   spellCheckEnabled: boolean,
 *   translateEnabled: boolean,
 * }}
 */
var LanguageState;

/**
 * Input method data to expose to consumers (Chrome OS only).
 * supported: an array of supported input methods set once at initialization.
 * enabled: an array of the currently enabled input methods.
 * currentId: ID of the currently active input method.
 * @typedef {{
 *     supported: !Array<!chrome.languageSettingsPrivate.InputMethod>,
 *     enabled: !Array<!chrome.languageSettingsPrivate.InputMethod>,
 *     currentId: string,
 * }}
 */
var InputMethodsModel;

/**
 * Languages data to expose to consumers.
 * supported: an array of languages, ordered alphabetically, set once
 *     at initialization.
 * enabled: an array of enabled language states, ordered by preference.
 * translateTarget: the default language to translate into.
 * inputMethods: the InputMethodsModel (Chrome OS only).
 * @typedef {{
 *   supported: !Array<!chrome.languageSettingsPrivate.Language>,
 *   enabled: !Array<!LanguageState>,
 *   translateTarget: string,
 *   inputMethods: (!InputMethodsModel|undefined),
 * }}
 */
var LanguagesModel;

/**
 * Helper methods implemented by settings-languages-singleton. The nature of
 * the interaction between the singleton Polymer element and the |languages|
 * properties kept in sync is hidden from the consumer, which can just treat
 * these methods as a handy interface.
 * @interface
 */
var LanguageHelper = function() {};

LanguageHelper.prototype = {

  /** @return {!Promise} */
  whenReady: assertNotReached,

  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   * @param {string} languageCode
   */
  setUILanguage: assertNotReached,

  /** Resets the prospective UI language back to the actual UI language. */
  resetUILanguage: assertNotReached,

  /**
   * Returns the "prospective" UI language, i.e. the one to be used on next
   * restart. If the pref is not set, the current UI language is also the
   * "prospective" language.
   * @return {string} Language code of the prospective UI language.
   */
  getProspectiveUILanguage: assertNotReached,

  /**
   * @param {string} languageCode
   * @return {boolean}
   */
  isLanguageEnabled: assertNotReached,

  /**
   * Enables the language, making it available for spell check and input.
   * @param {string} languageCode
   */
  enableLanguage: assertNotReached,

  /**
   * Disables the language.
   * @param {string} languageCode
   */
  disableLanguage: assertNotReached,

  /**
   * @param {string} languageCode Language code for an enabled language.
   * @return {boolean}
   */
  canDisableLanguage: assertNotReached,

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param {string} languageCode
   * @param {number} offset Negative offset moves the language toward the front
   *     of the list. A Positive one moves the language toward the back.
   */
  moveLanguage: assertNotReached,

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   * @param {string} languageCode
   */
  enableTranslateLanguage: assertNotReached,

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   * @param {string} languageCode
   */
  disableTranslateLanguage: assertNotReached,

  /**
   * Enables or disables spell check for the given language.
   * @param {string} languageCode
   * @param {boolean} enable
   */
  toggleSpellCheck: assertNotReached,

  /**
   * Converts the language code for translate. There are some differences
   * between the language set the Translate server uses and that for
   * Accept-Language.
   * @param {string} languageCode
   * @return {string} The converted language code.
   */
  convertLanguageCodeForTranslate: assertNotReached,

  /**
   * Given a language code, returns just the base language. E.g., converts
   * 'en-GB' to 'en'.
   * @param {string} languageCode
   * @return {string}
   */
  getLanguageCodeWithoutRegion: assertNotReached,

  /**
   * @param {string} languageCode
   * @return {!chrome.languageSettingsPrivate.Language|undefined}
   */
  getLanguage: assertNotReached,

  /**
   * @param {string} id
   * @return {!chrome.languageSettingsPrivate.InputMethod|undefined}
   */
  getInputMethod: assertNotReached,

  /** @param {string} id */
  addInputMethod: assertNotReached,

  /** @param {string} id */
  removeInputMethod: assertNotReached,

  /** @param {string} id */
  setCurrentInputMethod: assertNotReached,

  /**
   * param {string} languageCode
   * @return {!Array<!chrome.languageSettingsPrivate.InputMethod>}
   */
  getInputMethodsForLanguage: assertNotReached,

  /**
   * @param {!chrome.languageSettingsPrivate.InputMethod} inputMethod
   * @return {boolean}
   */
  isComponentIme: assertNotReached,

  /** @param {string} id Input method ID. */
  openInputMethodOptions: assertNotReached,

  /** @param {string} id New current input method ID. */
  onInputMethodChanged_: assertNotReached,

  /** @param {string} id Added input method ID. */
  onInputMethodAdded_: assertNotReached,

  /** @param {string} id Removed input method ID. */
  onInputMethodRemoved_: assertNotReached,
};
