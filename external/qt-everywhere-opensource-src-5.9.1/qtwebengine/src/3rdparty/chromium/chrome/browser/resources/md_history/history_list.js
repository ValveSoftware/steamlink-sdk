// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list',

  behaviors: [HistoryListBehavior],

  properties: {
    // The search term for the current query. Set when the query returns.
    searchedTerm: {
      type: String,
      value: '',
    },

    querying: Boolean,

    // An array of history entries in reverse chronological order.
    historyData_: Array,

    resultLoadingDisabled_: {
      type: Boolean,
      value: false,
    },

    lastFocused_: Object,
  },

  listeners: {
    'scroll': 'notifyListScroll_',
    'remove-bookmark-stars': 'removeBookmarkStars_',
  },

  /** @override */
  attached: function() {
    // It is possible (eg, when middle clicking the reload button) for all other
    // resize events to fire before the list is attached and can be measured.
    // Adding another resize here ensures it will get sized correctly.
    /** @type {IronListElement} */(this.$['infinite-list']).notifyResize();
    this.$['infinite-list'].scrollTarget = this;
    this.$['scroll-threshold'].scrollTarget = this;
  },

  /**
   * Remove bookmark star for history items with matching URLs.
   * @param {{detail: !string}} e
   * @private
   */
  removeBookmarkStars_: function(e) {
    var url = e.detail;

    if (this.historyData_ === undefined)
      return;

    for (var i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].url == url)
        this.set('historyData_.' + i + '.starred', false);
    }
  },

  /**
   * Disables history result loading when there are no more history results.
   */
  disableResultLoading: function() {
    this.resultLoadingDisabled_ = true;
  },

  /**
   * Adds the newly updated history results into historyData_. Adds new fields
   * for each result.
   * @param {!Array<!HistoryEntry>} historyResults The new history results.
   * @param {boolean} incremental Whether the result is from loading more
   * history, or a new search/list reload.
   */
  addNewResults: function(historyResults, incremental) {
    var results = historyResults.slice();
    /** @type {IronScrollThresholdElement} */(this.$['scroll-threshold'])
        .clearTriggers();

    if (!incremental) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData_)
        this.splice('historyData_', 0, this.historyData_.length);
      this.fire('unselect-all');
    }

    if (this.historyData_) {
      // If we have previously received data, push the new items onto the
      // existing array.
      results.unshift('historyData_');
      this.push.apply(this, results);
    } else {
      // The first time we receive data, use set() to ensure the iron-list is
      // initialized correctly.
      this.set('historyData_', results);
    }
  },

  /**
   * Called when the page is scrolled to near the bottom of the list.
   * @private
   */
  loadMoreData_: function() {
    if (this.resultLoadingDisabled_ || this.querying)
      return;

    this.fire('load-more-history');
  },

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {HistoryEntry} item
   * @param {number} index The index of |item| in |historyData_|.
   * @param {number} length The length of |historyData_|.
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(item, index, length) {
    return md_history.HistoryItem.needsTimeGap(
        this.historyData_, index, this.searchedTerm);
  },

  /**
   * True if the given item is the beginning of a new card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @param {number} length
   * @return {boolean}
   * @private
   */
  isCardStart_: function(item, i, length) {
    if (length == 0 || i > length - 1)
      return false;
    return i == 0 ||
        this.historyData_[i].dateRelativeDay !=
        this.historyData_[i - 1].dateRelativeDay;
  },

  /**
   * True if the given item is the end of a card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @param {number} length
   * @return {boolean}
   * @private
   */
  isCardEnd_: function(item, i, length) {
    if (length == 0 || i > length - 1)
      return false;
    return i == length - 1 ||
        this.historyData_[i].dateRelativeDay !=
        this.historyData_[i + 1].dateRelativeDay;
  },

  /**
   * @private
   */
  notifyListScroll_: function() {
    this.fire('history-list-scrolled');
  },

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  pathForItem_: function(index) {
    return 'historyData_.' + index;
  },
});
