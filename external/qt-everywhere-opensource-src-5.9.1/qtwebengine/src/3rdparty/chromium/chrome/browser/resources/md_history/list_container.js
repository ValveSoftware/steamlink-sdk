// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list-container',

  properties: {
    // The path of the currently selected page.
    selectedPage_: String,

    // Whether domain-grouped history is enabled.
    grouped: Boolean,

    /** @type {HistoryRange} */
    groupedRange: {type: Number, observer: 'groupedRangeChanged_'},

    /** @type {!QueryState} */
    queryState: Object,

    /** @type {!QueryResult} */
    queryResult: Object,
  },

  observers: [
    'searchTermChanged_(queryState.searchTerm)',
  ],

  listeners: {
    'history-list-scrolled': 'closeMenu_',
    'load-more-history': 'loadMoreHistory_',
    'toggle-menu': 'toggleMenu_',
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      Polymer.IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {
        text:
            md_history.HistoryItem.searchResultsTitle(results.length, info.term)
      });
    }

    if (this.selectedPage_ == 'grouped-list') {
      this.$$('#grouped-list').historyData = results;
      return;
    }

    var list = /** @type {HistoryListElement} */(this.$['infinite-list']);
    list.addNewResults(results, this.queryState.incremental);
    if (info.finished)
      list.disableResultLoading();
  },

  /**
   * Queries the history backend for results based on queryState.
   * @param {boolean} incremental Whether the new query should continue where
   *    the previous query stopped.
   */
  queryHistory: function(incremental) {
    var queryState = this.queryState;
    // Disable querying until the first set of results have been returned. If
    // there is a search, query immediately to support search query params from
    // the URL.
    var noResults = !this.queryResult || this.queryResult.results == null;
    if (queryState.queryingDisabled ||
        (!this.queryState.searchTerm && noResults)) {
      return;
    }

    // Close any open dialog if a new query is initiated.
    var dialog = this.$.dialog.getIfExists();
    if (!incremental && dialog && dialog.open)
      dialog.close();

    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);

    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryResult.results.slice(-1)[0];
      lastVisitTime = lastVisit ? Math.floor(lastVisit.time) : 0;
    }

    var maxResults =
      this.groupedRange == HistoryRange.ALL_TIME ? RESULTS_PER_PAGE : 0;
    chrome.send('queryHistory', [
      queryState.searchTerm, queryState.groupedOffset, queryState.range,
      lastVisitTime, maxResults
    ]);
  },

  historyDeleted: function() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0)
      return;

    // Reload the list with current search state.
    this.queryHistory(false);
  },

  /** @return {HTMLElement} */
  getContentScrollTarget: function() {
    return this.getSelectedList_();
  },

  /** @return {number} */
  getSelectedItemCount: function() {
    return this.getSelectedList_().selectedPaths.size;
  },

  unselectAllItems: function(count) {
    var selectedList = this.getSelectedList_();
    if (selectedList)
      selectedList.unselectAllItems(count);
  },

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory'))
      return;

    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm != '')
      browserService.recordAction('SearchResultRemove');
    this.$.dialog.get().showModal();

    // TODO(dbeam): remove focus flicker caused by showModal() + focus().
    this.$$('.action-button').focus();
  },

  /**
   * @param {HistoryRange} range
   * @private
   */
  groupedRangeChanged_: function(range, oldRange) {
    this.selectedPage_ = range == HistoryRange.ALL_TIME ?
      'infinite-list' : 'grouped-list';

    if (oldRange == undefined)
      return;

    this.queryHistory(false);
    this.fire('history-view-changed');
  },

  /** @private */
  searchTermChanged_: function() {
    this.queryHistory(false);
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm)
      md_history.BrowserService.getInstance().recordAction('Search');
  },

  /** @private */
  loadMoreHistory_: function() { this.queryHistory(true); },

  /**
   * @param {HistoryQuery} info
   * @param {!Array<HistoryEntry>} results
   * @private
   */
  initializeResults_: function(info, results) {
    if (results.length == 0)
      return;

    var currentDate = results[0].dateRelativeDay;

    for (var i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].readableTimestamp =
          info.term == '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay != currentDate) {
        currentDate = results[i].dateRelativeDay;
      }
    }
  },

  /** @private */
  onDialogConfirmTap_: function() {
    md_history.BrowserService.getInstance().recordAction(
        'ConfirmRemoveSelected');

    this.getSelectedList_().deleteSelected();
    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /** @private */
  onDialogCancelTap_: function() {
    md_history.BrowserService.getInstance().recordAction(
        'CancelRemoveSelected');

    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    var menu = this.$.sharedMenu.getIfExists();
    if (menu)
      menu.closeMenu();
  },

  /**
   * Opens the overflow menu unless the menu is already open and the same button
   * is pressed.
   * @param {{detail: {item: !HistoryEntry, target: !HTMLElement}}} e
   * @private
   */
  toggleMenu_: function(e) {
    var target = e.detail.target;
    var menu = /** @type {CrSharedMenuElement} */this.$.sharedMenu.get();
    menu.toggleMenu(target, e.detail);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    md_history.BrowserService.getInstance().recordAction(
        'EntryMenuShowMoreFromSite');

    var menu = assert(this.$.sharedMenu.getIfExists());
    this.set('queryState.searchTerm', menu.itemData.item.domain);
    menu.closeMenu();
  },

  /** @private */
  onRemoveFromHistoryTap_: function() {
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');
    var menu = assert(this.$.sharedMenu.getIfExists());
    var itemData = menu.itemData;
    browserService.deleteItems([itemData.item])
        .then(function(items) {
          // This unselect-all resets the toolbar when deleting a selected item
          // and clears selection state which can be invalid if items move
          // around during deletion.
          // TODO(tsergeant): Make this automatic based on observing list
          // modifications.
          this.fire('unselect-all');
          this.getSelectedList_().removeItemsByPath([itemData.path]);

          var index = itemData.index;
          if (index == undefined)
            return;

          var browserService = md_history.BrowserService.getInstance();
          browserService.recordHistogram(
              'HistoryPage.RemoveEntryPosition',
              Math.min(index, UMA_MAX_BUCKET_VALUE), UMA_MAX_BUCKET_VALUE);
          if (index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
            browserService.recordHistogram(
                'HistoryPage.RemoveEntryPositionSubset', index,
                UMA_MAX_SUBSET_BUCKET_VALUE);
          }
        }.bind(this));
    menu.closeMenu();
  },

  /**
   * @return {HTMLElement}
   * @private
   */
  getSelectedList_: function() { return this.$.content.selectedItem; },

  /** @private */
  canDeleteHistory_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  }
});
