// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-add-languages-dialog' is a dialog for enabling
 * languages.
 */
Polymer({
  is: 'settings-add-languages-dialog',

  properties: {
    /** @type {!LanguagesModel|undefined} */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /** @private {!Set<string>} */
    languagesToAdd_: {
      type: Object,
      value: function() { return new Set(); },
    },

    /** @private */
    disableActionButton_: {
      type: Boolean,
      value: true,
    },
  },

  attached: function() {
    this.$.dialog.showModal();
    // Fire iron-resize after the list initially displays to prevent flickering.
    setTimeout(function() {
      this.$$('iron-list').fire('iron-resize');
    }.bind(this));
  },

  /**
   * Returns the supported languages that are not yet enabled, based on
   * the LanguageHelper's enabled languages list.
   * @param {!Array<!chrome.languageSettingsPrivate.Language>}
   *     supportedLanguages
   * @param {!Object} enabledLanguagesChange Polymer change record for
   *     |enabledLanguages|.
   * @return {!Array<!chrome.languageSettingsPrivate.Language>}
   * @private
   */
  getAvailableLanguages_: function(supportedLanguages, enabledLanguagesChange) {
    return supportedLanguages.filter(function(language) {
      return !this.languageHelper.isLanguageEnabled(language.code);
    }.bind(this));
  },

  /**
   * True if the user has chosen to add this language (checked its checkbox).
   * @return {boolean}
   * @private
   */
  willAdd_: function(languageCode) {
    return this.languagesToAdd_.has(languageCode);
  },

  /**
   * Handler for checking or unchecking a language item.
   * @param {!{model: !{item: !chrome.languageSettingsPrivate.Language},
   *           target: !PaperCheckboxElement}} e
   * @private
   */
  onLanguageCheckboxChange_: function(e) {
    // Add or remove the item to the Set. No need to worry about data binding:
    // willAdd_ is called to initialize the checkbox state (in case the
    // iron-list re-uses a previous checkbox), and the checkbox can only be
    // changed after that by user action.
    var code = e.model.item.code;
    if (e.target.checked)
      this.languagesToAdd_.add(code);
    else
      this.languagesToAdd_.delete(code);

    this.disableActionButton_ = !this.languagesToAdd_.size;
  },

  /** @private */
  onCancelButtonTap_: function() {
    this.$.dialog.close();
  },

  /**
   * Enables the checked languages.
   * @private
   */
  onActionButtonTap_: function() {
    this.$.dialog.close();
    for (var languageCode of this.languagesToAdd_)
      this.languageHelper.enableLanguage(languageCode);
  },
});
