// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.dictionary_words', function() {
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;

  /**
   * Creates a new dictionary word list item.
   * @param {string} dictionaryWord The dictionary word.
   * @param {function(string)} addDictionaryWordCallback Callback for
   * adding a dictionary word.
   * @constructor
   * @extends {cr.ui.InlineEditableItem}
   */
  function DictionaryWordsListItem(dictionaryWord, addDictionaryWordCallback) {
    var el = cr.doc.createElement('div');
    el.dictionaryWord_ = dictionaryWord;
    el.addDictionaryWordCallback_ = addDictionaryWordCallback;
    DictionaryWordsListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an HTML element as a dictionary word list item.
   * @param {HTMLElement} el The element to decorate.
   */
  DictionaryWordsListItem.decorate = function(el) {
    el.__proto__ = DictionaryWordsListItem.prototype;
    el.decorate();
  };

  DictionaryWordsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);
      if (this.dictionaryWord_ == '')
        this.isPlaceholder = true;
      else
        this.editable = false;

      var wordEl = this.createEditableTextCell(this.dictionaryWord_);
      wordEl.classList.add('weakrtl');
      wordEl.classList.add('language-dictionary-overlay-word-list-item');
      this.contentElement.appendChild(wordEl);

      var wordField = wordEl.querySelector('input');
      wordField.classList.add('language-dictionary-overlay-word-list-item');
      if (this.isPlaceholder) {
        wordField.placeholder =
            loadTimeData.getString('languageDictionaryOverlayAddWordLabel');
      }

      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    /** @override */
    get hasBeenEdited() {
      return this.querySelector('input').value.length > 0;
    },

    /**
     * Adds a word to the dictionary.
     * @param {Event} e Edit committed event.
     * @private
     */
    onEditCommitted_: function(e) {
      var input = e.currentTarget.querySelector('input');
      this.addDictionaryWordCallback_(input.value);
      input.value = '';
    },
  };

  /**
   * A list of words in the dictionary.
   * @extends {cr.ui.InlineEditableItemList}
   */
  var DictionaryWordsList = cr.ui.define('list');

  DictionaryWordsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * The function to notify that the word list has changed.
     * @type {function()}
     */
    onWordListChanged: null,

    /**
     * The list of all words in the dictionary. Used to generate a filtered word
     * list in the |search(searchTerm)| method.
     * @type {Array}
     * @private
     */
    allWordsList_: null,

    /**
     * The list of words that the user removed, but |DictionaryWordList| has not
     * received a notification of their removal yet.
     * @type {Array}
     * @private
     */
    removedWordsList_: [],

    /**
     * Adds a dictionary word.
     * @param {string} dictionaryWord The word to add.
     * @private
     */
    addDictionaryWord_: function(dictionaryWord) {
      this.allWordsList_.push(dictionaryWord);
      this.dataModel.splice(this.dataModel.length - 1, 0, dictionaryWord);
      this.onWordListChanged();
      chrome.send('addDictionaryWord', [dictionaryWord]);
    },

    /**
     * Searches the list for the matching words.
     * @param {string} searchTerm The search term.
     */
    search: function(searchTerm) {
      var filteredWordList = this.allWordsList_.filter(
          function(element, index, array) {
            return element.indexOf(searchTerm) > -1;
          });
      filteredWordList.push('');
      this.dataModel = new cr.ui.ArrayDataModel(filteredWordList);
      this.onWordListChanged();
    },

    /**
     * Sets the list of dictionary words.
     * @param {Array} entries The list of dictionary words.
     */
    setWordList: function(entries) {
      this.allWordsList_ = entries.slice(0);
      // Empty string is a placeholder for entering new words.
      entries.push('');
      this.dataModel = new cr.ui.ArrayDataModel(entries);
      this.onWordListChanged();
    },

    /**
     * Adds non-duplicate dictionary words.
     * @param {Array} entries The list of dictionary words.
     */
    addWords: function(entries) {
      var toAdd = [];
      for (var i = 0; i < entries.length; i++) {
        if (this.allWordsList_.indexOf(entries[i]) == -1) {
          this.allWordsList_.push(entries[i]);
          toAdd.push(entries[i]);
        }
      }
      if (toAdd.length == 0)
        return;
      for (var i = 0; i < toAdd.length; i++)
        this.dataModel.splice(this.dataModel.length - 1, 0, toAdd[i]);
      this.onWordListChanged();
    },

    /**
     * Removes dictionary words that are not in |removedWordsList_|. If a word
     * is in |removedWordsList_|, then removes the word from there instead.
     * @param {Array} entries The list of dictionary words.
     */
    removeWords: function(entries) {
      var index;
      var toRemove = [];
      for (var i = 0; i < entries.length; i++) {
        index = this.removedWordsList_.indexOf(entries[i]);
        if (index > -1) {
          this.removedWordsList_.splice(index, 1);
        } else {
          index = this.allWordsList_.indexOf(entries[i]);
          if (index > -1) {
            this.allWordsList_.splice(index, 1);
            toRemove.push(entries[i]);
          }
        }
      }
      if (toRemove.length == 0)
        return;
      for (var i = 0; i < toRemove.length; i++) {
        index = this.dataModel.indexOf(toRemove[i]);
        if (index > -1)
          this.dataModel.splice(index, 1);
      }
      this.onWordListChanged();
    },

    /**
     * Returns true if the data model contains no words, otherwise returns
     * false.
     * @type {boolean}
     */
    get empty() {
      // A data model without dictionary words contains one placeholder for
      // entering new words.
      return this.dataModel.length < 2;
    },

    /** @override */
    createItem: function(dictionaryWord) {
      return new DictionaryWordsListItem(
          dictionaryWord,
          this.addDictionaryWord_.bind(this));
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      // The last element in the data model is an undeletable placeholder for
      // entering new words.
      assert(index > -1 && index < this.dataModel.length - 1);
      var item = this.dataModel.item(index);
      var allWordsListIndex = this.allWordsList_.indexOf(item);
      assert(allWordsListIndex > -1);
      this.allWordsList_.splice(allWordsListIndex, 1);
      this.dataModel.splice(index, 1);
      this.removedWordsList_.push(item);
      this.onWordListChanged();
      chrome.send('removeDictionaryWord', [item]);
    },

    /** @override */
    shouldFocusPlaceholder: function() {
      return false;
    },
  };

  return {
    DictionaryWordsList: DictionaryWordsList
  };

});

