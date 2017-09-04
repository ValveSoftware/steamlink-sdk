// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */
Polymer({
  is: 'settings-edit-dictionary-page',

  properties: {
    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: function() { return []; },
    },
  },

  ready: function() {
    chrome.languageSettingsPrivate.getSpellcheckWords(function(words) {
      this.words_ = words;
    }.bind(this));

    // Updates are applied locally so they appear immediately, but we should
    // listen for changes in case they come from elsewhere.
    chrome.languageSettingsPrivate.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the paper-input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * Handles updates to the word list. Additions triggered by this element are
   * de-duped so the word list remains a set. Words are appended to the end
   * instead of re-sorting the list so it's clear what words were added.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_: function(added, removed) {
    for (var i = 0; i < removed.length; i++)
      this.arrayDelete('words_', removed[i]);

    for (var i = 0; i < added.length; i++) {
      if (this.words_.indexOf(added[i]) == -1)
        this.push('words_', added[i]);
    }
  },

  /**
   * Handles Enter and Escape key presses for the paper-input.
   * @param {!{detail: !{key: string}}} e
   */
  onKeysPress_: function(e) {
    if (e.detail.key == 'enter')
      this.addWordFromInput_();
    else if (e.detail.key == 'esc')
      e.detail.keyboardEvent.target.value = '';
  },

  /**
   * Handles tapping on the Add Word button.
   */
  onAddWordTap_: function(e) {
    this.addWordFromInput_();
    this.$.newWord.focus();
  },

  /**
   * Handles tapping on a paper-item's Remove Word icon button.
   * @param {!{model: !{item: string}}} e
   */
  onRemoveWordTap_: function(e) {
    chrome.languageSettingsPrivate.removeSpellcheckWord(e.model.item);
    this.arrayDelete('words_', e.model.item);
  },

  /**
   * Adds the word in the paper-input to the dictionary, also appending it
   * to the end of the list of words shown to the user.
   */
  addWordFromInput_: function() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    var word = this.$.newWord.value.trim();
    this.$.newWord.value = '';
    if (!word)
      return;

    var index = this.words_.indexOf(word);
    if (index == -1) {
      chrome.languageSettingsPrivate.addSpellcheckWord(word);
      this.push('words_', word);
    }

    // Scroll to the word (usually the bottom, or to the index if the word
    // is already present).
    this.$.list.scrollToIndex(index);
  },
});
