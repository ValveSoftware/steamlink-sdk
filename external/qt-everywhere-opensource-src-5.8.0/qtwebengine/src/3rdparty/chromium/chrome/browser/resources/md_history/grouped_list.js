// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{domain: string,
 *            visits: !Array<HistoryEntry>,
 *            rendered: boolean,
 *            expanded: boolean}}
 */
var HistoryDomain;

/**
 * @typedef {{title: string,
 *            domains: !Array<HistoryDomain>}}
 */
var HistoryGroup;

// TODO(calamity): Support selection by refactoring selection out of
// history-list and into history-app.
Polymer({
  is: 'history-grouped-list',

  properties: {
    // An array of history entries in reverse chronological order.
    historyData: {
      type: Array,
    },

    /**
     * @type {Array<HistoryGroup>}
     */
    groupedHistoryData_: {
      type: Array,
    },

    searchedTerm: {
      type: String,
      value: ''
    },

    range: {
      type: Number,
    },

    queryStartTime: String,
    queryEndTime: String,
  },

  observers: [
    'updateGroupedHistoryData_(range, historyData)'
  ],

  /**
   * Make a list of domains from visits.
   * @param {!Array<!HistoryEntry>} visits
   * @return {!Array<!HistoryDomain>}
   */
  createHistoryDomains_: function(visits) {
    var domainIndexes = {};
    var domains = [];

    // Group the visits into a dictionary and generate a list of domains.
    for (var i = 0, visit; visit = visits[i]; i++) {
      var domain = visit.domain;
      if (domainIndexes[domain] == undefined) {
        domainIndexes[domain] = domains.length;
        domains.push({
          domain: domain,
          visits: [],
          expanded: false,
          rendered: false,
        });
      }
      domains[domainIndexes[domain]].visits.push(visit);
    }
    var sortByVisits = function(a, b) {
      return b.visits.length - a.visits.length;
    };
    domains.sort(sortByVisits);

    return domains;
  },

  updateGroupedHistoryData_: function() {
    if (this.historyData.length == 0) {
      this.groupedHistoryData_ = [];
      return;
    }

    if (this.range == HistoryRange.WEEK) {
      // Group each day into a list of results.
      var days = [];
      var currentDayVisits = [this.historyData[0]];

      var pushCurrentDay = function() {
        days.push({
          title: this.searchedTerm ? currentDayVisits[0].dateShort :
              currentDayVisits[0].dateRelativeDay,
          domains: this.createHistoryDomains_(currentDayVisits),
        });
      }.bind(this);

      var visitsSameDay = function(a, b) {
        if (this.searchedTerm)
          return a.dateShort == b.dateShort;

        return a.dateRelativeDay == b.dateRelativeDay;
      }.bind(this);

      for (var i = 1; i < this.historyData.length; i++) {
        var visit = this.historyData[i];
        if (!visitsSameDay(visit, currentDayVisits[0])) {
          pushCurrentDay();
          currentDayVisits = [];
        }
        currentDayVisits.push(visit);
      }
      pushCurrentDay();

      this.groupedHistoryData_ = days;
    } else if (this.range == HistoryRange.MONTH) {
      // Group each all visits into a single list.
      this.groupedHistoryData_ = [{
        title: this.queryStartTime + ' – ' + this.queryEndTime,
        domains: this.createHistoryDomains_(this.historyData)
      }];
    }
  },

  /**
   * @param {{model:Object, currentTarget:IronCollapseElement}} e
   */
  toggleDomainExpanded_: function(e) {
    var collapse = e.currentTarget.parentNode.querySelector('iron-collapse');
    e.model.set('domain.rendered', true);

    // Give the history-items time to render.
    setTimeout(function() { collapse.toggle() }, 0);
  },

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {number} groupIndex
   * @param {number} domainIndex
   * @param {number} itemIndex
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(groupIndex, domainIndex, itemIndex) {
    var visits =
        this.groupedHistoryData_[groupIndex].domains[domainIndex].visits;

    return md_history.HistoryItem.needsTimeGap(
        visits, itemIndex, this.searchedTerm);
  },

  hasResults_: function(historyDataLength) {
    return historyDataLength > 0;
  },

  getWebsiteIconStyle_: function(domain) {
    return 'background-image: ' +
        cr.icon.getFaviconImageSet(domain.visits[0].url);
  },

  getDropdownIcon_: function(expanded) {
    return expanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  noResultsMessage_: function(searchedTerm) {
    var messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },
});
