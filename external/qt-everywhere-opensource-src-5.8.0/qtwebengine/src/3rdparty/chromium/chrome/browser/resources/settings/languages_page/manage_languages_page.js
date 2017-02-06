// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-manage-languages-page' is a sub-page for enabling
 * and disabling languages.
 */
Polymer({
  is: 'settings-manage-languages-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /** @private {!LanguageHelper} */
    languageHelper_: Object,
  },

  /** @override */
  created: function() {
     this.languageHelper_ = LanguageHelperImpl.getInstance();
  },

  /**
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @param {!Object} change Polymer change object (provided in the HTML so this
   *     gets called whenever languages.enabled.* changes).
   * @return {boolean}
   * @private
   */
  isCheckboxChecked_: function(language, change) {
    return this.languageHelper_.isLanguageEnabled(language.code);
  },

  /**
   * Determines whether a language must be enabled. If so, the checkbox in the
   * available languages list should not be changeable.
   * @param {!chrome.languageSettingsPrivate.Language} language
   * @param {!Object} change Polymer change object (provided in the HTML so this
   *     gets called whenever languages.enabled.* changes).
   * @return {boolean}
   * @private
   */
  isLanguageRequired_: function(language, change) {
    // This check only applies to enabled languages.
    if (!this.languageHelper_.isLanguageEnabled(language.code))
      return false;
    return !this.languageHelper_.canDisableLanguage(language.code);
  },

  /**
   * Handler for removing a language.
   * @param {!{model: !{item: !LanguageState}}} e
   * @private
   */
  onRemoveLanguageTap_: function(e) {
    this.languageHelper_.disableLanguage(e.model.item.language.code);
  },

  /**
   * Handler for checking or unchecking a language item.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.Language},
   *           target: !PaperCheckboxElement}} e
   * @private
   */
  onLanguageCheckboxChange_: function(e) {
    var code = e.model.item.code;
    if (e.target.checked)
      this.languageHelper_.enableLanguage(code);
    else
      this.languageHelper_.disableLanguage(code);
  },
});
