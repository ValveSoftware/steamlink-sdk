// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implements an incremental search field which can be shown and hidden.
 * Canonical implementation is <cr-search-field>.
 * @polymerBehavior
 */
var CrSearchFieldBehavior = {
  properties: {
    label: {
      type: String,
      value: '',
    },

    clearLabel: {
      type: String,
      value: '',
    },

    showingSearch: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'showingSearchChanged_',
      reflectToAttribute: true
    },

    /** @private */
    lastValue_: {
      type: String,
      value: '',
    },
  },

  /**
   * @return {string} The value of the search field.
   */
  getValue: function() {
    return this.$.searchInput.value;
  },

  /**
   * Sets the value of the search field.
   * @param {string} value
   */
  setValue: function(value) {
    // Use bindValue when setting the input value so that changes propagate
    // correctly.
    this.$.searchInput.bindValue = value;
    this.onValueChanged_(value);
  },

  showAndFocus: function() {
    this.showingSearch = true;
    this.focus_();
  },

  /** @private */
  focus_: function() {
    this.$.searchInput.focus();
  },

  onSearchTermSearch: function() {
    this.onValueChanged_(this.getValue());
  },

  /**
   * Updates the internal state of the search field based on a change that has
   * already happened.
   * @param {string} newValue
   * @private
   */
  onValueChanged_: function(newValue) {
    if (newValue == this.lastValue_)
      return;

    this.fire('search-changed', newValue);
    this.lastValue_ = newValue;
  },

  onSearchTermKeydown: function(e) {
    if (e.key == 'Escape')
      this.showingSearch = false;
  },

  /** @private */
  showingSearchChanged_: function() {
    if (this.showingSearch) {
      this.focus_();
      return;
    }

    this.setValue('');
    this.$.searchInput.blur();
  },

  /** @private */
  toggleShowingSearch_: function() {
    this.showingSearch = !this.showingSearch;
  },
};
