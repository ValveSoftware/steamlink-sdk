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
   * @abstract
   * @return {!HTMLInputElement} The input field element the behavior should
   * use.
   */
  getSearchInput: function() {},

  /**
   * @return {string} The value of the search field.
   */
  getValue: function() {
    return this.getSearchInput().value;
  },

  /**
   * Sets the value of the search field.
   * @param {string} value
   * @param {boolean=} opt_noEvent Whether to prevent a 'search-changed' event
   *     firing for this change.
   */
  setValue: function(value, opt_noEvent) {
    var searchInput = this.getSearchInput();
    searchInput.value = value;
    // If the input is an iron-input, ensure that the new value propagates as
    // expected.
    if (searchInput.bindValue != undefined)
      searchInput.bindValue = value;
    this.onValueChanged_(value, !!opt_noEvent);
  },

  showAndFocus: function() {
    this.showingSearch = true;
    this.focus_();
  },

  /** @private */
  focus_: function() {
    this.getSearchInput().focus();
  },

  onSearchTermSearch: function() {
    this.onValueChanged_(this.getValue(), false);
  },

  /**
   * Updates the internal state of the search field based on a change that has
   * already happened.
   * @param {string} newValue
   * @param {boolean} noEvent Whether to prevent a 'search-changed' event firing
   *     for this change.
   * @private
   */
  onValueChanged_: function(newValue, noEvent) {
    if (newValue == this.lastValue_)
      return;

    this.lastValue_ = newValue;

    if (!noEvent)
      this.fire('search-changed', newValue);
  },

  onSearchTermKeydown: function(e) {
    if (e.key == 'Escape')
      this.showingSearch = false;
  },

  /**
   * @param {boolean} current
   * @param {boolean|undefined} previous
   * @private
   */
  showingSearchChanged_: function(current, previous) {
    // Prevent unnecessary 'search-changed' event from firing on startup.
    if (previous == undefined)
      return;

    if (this.showingSearch) {
      this.focus_();
      return;
    }

    this.setValue('');
    this.getSearchInput().blur();
  }
};
