// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list',

  properties: {
    // The search term for the current query. Set when the query returns.
    searchedTerm: {
      type: String,
      value: '',
    },

    lastSearchedTerm_: String,

    querying: Boolean,

    // An array of history entries in reverse chronological order.
    historyData_: Array,

    resultLoadingDisabled_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'infinite-list.scroll': 'closeMenu_',
    'tap': 'closeMenu_',
    'toggle-menu': 'toggleMenu_',
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).closeMenu();
  },

  /**
   * Opens the overflow menu unless the menu is already open and the same button
   * is pressed.
   * @param {{detail: {item: !HistoryEntry, target: !HTMLElement}}} e
   * @private
   */
  toggleMenu_: function(e) {
    var target = e.detail.target;
    /** @type {CrSharedMenuElement} */(this.$.sharedMenu).toggleMenu(
        target, e.detail.item);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.sharedMenu);
    this.fire('search-domain', {domain: menu.itemData.domain});
    menu.closeMenu();
  },

  /** @private */
  onRemoveFromHistoryTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.sharedMenu);
    md_history.BrowserService.getInstance()
        .deleteItems([menu.itemData])
        .then(function(items) {
          this.removeDeletedHistory_(items);
          // This unselect-all is to reset the toolbar when deleting a selected
          // item. TODO(tsergeant): Make this automatic based on observing list
          // modifications.
          this.fire('unselect-all');
        }.bind(this));
    menu.closeMenu();
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
   */
  addNewResults: function(historyResults) {
    var results = historyResults.slice();
    /** @type {IronScrollThresholdElement} */(this.$['scroll-threshold'])
        .clearTriggers();

    if (this.lastSearchedTerm_ != this.searchedTerm) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData_)
        this.splice('historyData_', 0, this.historyData_.length);
      this.fire('unselect-all');
      this.lastSearchedTerm_ = this.searchedTerm;
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
   * Cycle through each entry in historyData_ and set all items to be
   * unselected.
   * @param {number} overallItemCount The number of checkboxes selected.
   */
  unselectAllItems: function(overallItemCount) {
    if (this.historyData_ === undefined)
      return;

    for (var i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].selected) {
        this.set('historyData_.' + i + '.selected', false);
        overallItemCount--;
        if (overallItemCount == 0)
          break;
      }
    }
  },

  /**
   * Remove the given |items| from the list. Expected to be called after the
   * items are removed from the backend.
   * @param {!Array<!HistoryEntry>} removalList
   * @private
   */
  removeDeletedHistory_: function(removalList) {
    // This set is only for speed. Note that set inclusion for objects is by
    // reference, so this relies on the HistoryEntry objects never being copied.
    var deletedItems = new Set(removalList);
    var splices = [];

    for (var i = this.historyData_.length - 1; i >= 0; i--) {
      var item = this.historyData_[i];
      if (deletedItems.has(item)) {
        // Removes the selected item from historyData_. Use unshift so
        // |splices| ends up in index order.
        splices.unshift({
          index: i,
          removed: [item],
          addedCount: 0,
          object: this.historyData_,
          type: 'splice'
        });
        this.historyData_.splice(i, 1);
      }
    }
    // notifySplices gives better performance than individually splicing as it
    // batches all of the updates together.
    this.notifySplices('historyData_', splices);
  },

  /**
   * Performs a request to the backend to delete all selected items. If
   * successful, removes them from the view.
   */
  deleteSelected: function() {
    var toBeRemoved = this.historyData_.filter(function(item) {
      return item.selected;
    });
    md_history.BrowserService.getInstance()
        .deleteItems(toBeRemoved)
        .then(function(items) {
          this.removeDeletedHistory_(items);
          this.fire('unselect-all');
        }.bind(this));
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

  hasResults: function(historyDataLength) {
    return historyDataLength > 0;
  },

  noResultsMessage_: function(searchedTerm, isLoading) {
    if (isLoading)
      return '';
    var messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
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
   * @param {number} index
   * @return {boolean}
   * @private
   */
  isFirstItem_: function(index) {
    return index == 0;
  }
});
