// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{querying: boolean,
 *            searchTerm: string,
 *            results: ?Array<!HistoryEntry>,
 *            info: ?HistoryQuery,
 *            incremental: boolean,
 *            range: HistoryRange,
 *            groupedOffset: number,
 *            sessionList: ?Array<!ForeignSession>}}
 */
var QueryState;

Polymer({
  is: 'history-app',

  properties: {
    // The id of the currently selected page.
    selectedPage_: {
      type: String,
      value: 'history-list',
      observer: 'unselectAll'
    },

    // Whether domain-grouped history is enabled.
    grouped_: {
      type: Boolean,
      reflectToAttribute: true
    },

    // Whether the first set of results have returned.
    firstLoad_: { type: Boolean, value: true },

    // True if the history queries are disabled.
    queryingDisabled_: Boolean,

    /** @type {!QueryState} */
    // TODO(calamity): Split out readOnly data into a separate property which is
    // only set on result return.
    queryState_: {
      type: Object,
      value: function() {
        return {
          // A query is initiated by page load.
          querying: true,
          searchTerm: '',
          results: null,
          // Whether the most recent query was incremental.
          incremental: false,
          info: null,
          range: HistoryRange.ALL_TIME,
          // TODO(calamity): Make history toolbar buttons change the offset.
          groupedOffset: 0,
          sessionList: null,
        };
      }
    },
  },

  observers: [
    'searchTermChanged_(queryState_.searchTerm)',
    'groupedRangeChanged_(queryState_.range)',
  ],

  // TODO(calamity): Replace these event listeners with data bound properties.
  listeners: {
    'cr-menu-tap': 'onMenuTap_',
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-domain': 'searchDomain_',
    'load-more-history': 'loadMoreHistory_',
  },

  /** @override */
  ready: function() {
    this.grouped_ = loadTimeData.getBoolean('groupByDomain');
  },

  /** @private */
  onMenuTap_: function() {
    this.$['side-bar'].toggle();
  },

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   * @param {{detail: {countAddition: number}}} e
   */
  checkboxSelected: function(e) {
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    toolbar.count += e.detail.countAddition;
  },

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   * @private
   */
  unselectAll: function() {
    var historyList =
        /** @type {HistoryListElement} */(this.$['history-list']);
    var toolbar = /** @type {HistoryToolbarElement} */(this.$.toolbar);
    historyList.unselectAllItems(toolbar.count);
    toolbar.count = 0;
  },

  /**
   * Listens for call to delete all selected items and loops through all items
   * to determine which ones are selected and deletes these.
   */
  deleteSelected: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory'))
      return;

    // TODO(hsampson): add a popup to check whether the user definitely
    // wants to delete the selected items.
    /** @type {HistoryListElement} */(this.$['history-list']).deleteSelected();
  },

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

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.firstLoad_ = false;
    this.set('queryState_.info', info);
    this.set('queryState_.results', results);
    this.set('queryState_.querying', false);

    this.initializeResults_(info, results);

    if (this.grouped_ && this.queryState_.range != HistoryRange.ALL_TIME) {
      this.$$('history-grouped-list').historyData = results;
      return;
    }

    var list = /** @type {HistoryListElement} */(this.$['history-list']);
    list.addNewResults(results);
    if (info.finished)
      list.disableResultLoading();
  },

  /**
   * Fired when the user presses 'More from this site'.
   * @param {{detail: {domain: string}}} e
   */
  searchDomain_: function(e) {
    this.$.toolbar.setSearchTerm(e.detail.domain);
  },

  searchTermChanged_: function(searchTerm) {
    this.queryHistory(false);
  },

  groupedRangeChanged_: function(range) {
    this.queryHistory(false);
  },

  loadMoreHistory_: function() {
    this.queryHistory(true);
  },

  /**
   * Queries the history backend for results based on queryState_.
   * @param {boolean} incremental Whether the new query should continue where
   *    the previous query stopped.
   */
  queryHistory: function(incremental) {
    if (this.queryingDisabled_ || this.firstLoad_)
      return;

    this.set('queryState_.querying', true);
    this.set('queryState_.incremental', incremental);

    var queryState = this.queryState_;

    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = queryState.results.slice(-1)[0];
      lastVisitTime = lastVisit ? lastVisit.time : 0;
    }

    var maxResults =
      queryState.range == HistoryRange.ALL_TIME ? RESULTS_PER_PAGE : 0;
    chrome.send('queryHistory', [
      queryState.searchTerm, queryState.groupedOffset, Number(queryState.range),
      lastVisitTime, maxResults
    ]);
  },

  /**
   * @param {!Array<!ForeignSession>} sessionList Array of objects describing
   *     the sessions from other devices.
   * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
   */
  setForeignSessions: function(sessionList, isTabSyncEnabled) {
    if (!isTabSyncEnabled)
      return;

    this.set('queryState_.sessionList', sessionList);
  },

  /**
   * @param {string} selectedPage
   * @param {HistoryRange} range
   * @return {string}
   */
  getSelectedPage: function(selectedPage, range) {
    if (selectedPage == 'history-list' && range != HistoryRange.ALL_TIME)
      return 'history-grouped-list';

    return selectedPage;
  },

  /**
   * Update sign in state of synced device manager after user logs in or out.
   * @param {boolean} isUserSignedIn
   */
  updateSignInState: function(isUserSignedIn) {
    var syncedDeviceManagerElem =
      /** @type {HistorySyncedDeviceManagerElement} */this
          .$$('history-synced-device-manager');
    syncedDeviceManagerElem.updateSignInState(isUserSignedIn);
  },

  /**
   * @param {string} selectedPage
   * @return {boolean}
   * @private
   */
  syncedTabsSelected_: function(selectedPage) {
    return selectedPage == 'history-synced-device-manager';
  },

  /**
   * @param {boolean} querying
   * @param {boolean} incremental
   * @param {string} searchTerm
   * @return {boolean} Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   * @private
   */
  shouldShowSpinner_: function(querying, incremental, searchTerm) {
    return querying && !incremental && searchTerm != '';
  }
});
