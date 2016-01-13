// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">
<include src="history_focus_manager.js">

///////////////////////////////////////////////////////////////////////////////
// Globals:
/** @const */ var RESULTS_PER_PAGE = 150;

// Amount of time between pageviews that we consider a 'break' in browsing,
// measured in milliseconds.
/** @const */ var BROWSING_GAP_TIME = 15 * 60 * 1000;

// The largest bucket value for UMA histogram, based on entry ID. All entries
// with IDs greater than this will be included in this bucket.
/** @const */ var UMA_MAX_BUCKET_VALUE = 1000;

// The largest bucket value for a UMA histogram that is a subset of above.
/** @const */ var UMA_MAX_SUBSET_BUCKET_VALUE = 100;

// TODO(glen): Get rid of these global references, replace with a controller
//     or just make the classes own more of the page.
var historyModel;
var historyView;
var pageState;
var selectionAnchor = -1;
var activeVisit = null;

/** @const */ var Command = cr.ui.Command;
/** @const */ var Menu = cr.ui.Menu;
/** @const */ var MenuButton = cr.ui.MenuButton;

/**
 * Enum that shows the filtering behavior for a host or URL to a managed user.
 * Must behave like the FilteringBehavior enum from managed_mode_url_filter.h.
 * @enum {number}
 */
ManagedModeFilteringBehavior = {
  ALLOW: 0,
  WARN: 1,
  BLOCK: 2
};

MenuButton.createDropDownArrows();

/**
 * Returns true if the mobile (non-desktop) version is being shown.
 * @return {boolean} true if the mobile version is being shown.
 */
function isMobileVersion() {
  return !document.body.classList.contains('uber-frame');
}

/**
 * Record an action in UMA.
 * @param {string} actionDesc The name of the action to be logged.
 */
function recordUmaAction(actionDesc) {
  chrome.send('metricsHandler:recordAction', [actionDesc]);
}

/**
 * Record a histogram value in UMA. If specified value is larger than the max
 * bucket value, record the value in the largest bucket.
 * @param {string} histogram The name of the histogram to be recorded in.
 * @param {integer} maxBucketValue The max value for the last histogram bucket.
 * @param {integer} value The value to record in the histogram.
 */

function recordUmaHistogram(histogram, maxBucketValue, value) {
  chrome.send('metricsHandler:recordInHistogram',
              [histogram,
              ((value > maxBucketValue) ? maxBucketValue : value),
              maxBucketValue]);
}

///////////////////////////////////////////////////////////////////////////////
// Visit:

/**
 * Class to hold all the information about an entry in our model.
 * @param {Object} result An object containing the visit's data.
 * @param {boolean} continued Whether this visit is on the same day as the
 *     visit before it.
 * @param {HistoryModel} model The model object this entry belongs to.
 * @constructor
 */
function Visit(result, continued, model) {
  this.model_ = model;
  this.title_ = result.title;
  this.url_ = result.url;
  this.domain_ = result.domain;
  this.starred_ = result.starred;

  // These identify the name and type of the device on which this visit
  // occurred. They will be empty if the visit occurred on the current device.
  this.deviceName = result.deviceName;
  this.deviceType = result.deviceType;

  // The ID will be set according to when the visit was displayed, not
  // received. Set to -1 to show that it has not been set yet.
  this.id_ = -1;

  this.isRendered = false;  // Has the visit already been rendered on the page?

  // All the date information is public so that owners can compare properties of
  // two items easily.

  this.date = new Date(result.time);

  // See comment in BrowsingHistoryHandler::QueryComplete - we won't always
  // get all of these.
  this.dateRelativeDay = result.dateRelativeDay || '';
  this.dateTimeOfDay = result.dateTimeOfDay || '';
  this.dateShort = result.dateShort || '';

  // Shows the filtering behavior for that host (only used for managed users).
  // A value of |ManagedModeFilteringBehavior.ALLOW| is not displayed so it is
  // used as the default value.
  this.hostFilteringBehavior = ManagedModeFilteringBehavior.ALLOW;
  if (typeof result.hostFilteringBehavior != 'undefined')
    this.hostFilteringBehavior = result.hostFilteringBehavior;

  this.blockedVisit = result.blockedVisit || false;

  // Whether this is the continuation of a previous day.
  this.continued = continued;

  this.allTimestamps = result.allTimestamps;
}

// Visit, public: -------------------------------------------------------------

/**
 * Returns a dom structure for a browse page result or a search page result.
 * @param {Object} propertyBag A bag of configuration properties, false by
 * default:
 *  - isSearchResult: Whether or not the result is a search result.
 *  - addTitleFavicon: Whether or not the favicon should be added.
 *  - useMonthDate: Whether or not the full date should be inserted (used for
 * monthly view).
 * @return {Node} A DOM node to represent the history entry or search result.
 */
Visit.prototype.getResultDOM = function(propertyBag) {
  var isSearchResult = propertyBag.isSearchResult || false;
  var addTitleFavicon = propertyBag.addTitleFavicon || false;
  var useMonthDate = propertyBag.useMonthDate || false;
  var node = createElementWithClassName('li', 'entry');
  var time = createElementWithClassName('div', 'time');
  var entryBox = createElementWithClassName('label', 'entry-box');
  var domain = createElementWithClassName('div', 'domain');

  this.id_ = this.model_.nextVisitId_++;

  // Only create the checkbox if it can be used either to delete an entry or to
  // block/allow it.
  if (this.model_.editingEntriesAllowed) {
    var checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = 'checkbox-' + this.id_;
    checkbox.time = this.date.getTime();
    checkbox.addEventListener('click', checkboxClicked);
    entryBox.appendChild(checkbox);

    // Clicking anywhere in the entryBox will check/uncheck the checkbox.
    entryBox.setAttribute('for', checkbox.id);
    entryBox.addEventListener('mousedown', entryBoxMousedown);
    entryBox.addEventListener('click', entryBoxClick);
  }

  // Keep track of the drop down that triggered the menu, so we know
  // which element to apply the command to.
  // TODO(dubroy): Ideally we'd use 'activate', but MenuButton swallows it.
  var self = this;
  var setActiveVisit = function(e) {
    activeVisit = self;
    var menu = $('action-menu');
    menu.dataset.devicename = self.deviceName;
    menu.dataset.devicetype = self.deviceType;
  };
  domain.textContent = this.domain_;

  entryBox.appendChild(time);

  var bookmarkSection = createElementWithClassName('div', 'bookmark-section');
  if (this.starred_) {
    bookmarkSection.classList.add('starred');
    bookmarkSection.addEventListener('click', function f(e) {
      recordUmaAction('HistoryPage_BookmarkStarClicked');
      bookmarkSection.classList.remove('starred');
      chrome.send('removeBookmark', [self.url_]);
      bookmarkSection.removeEventListener('click', f);
      e.preventDefault();
    });
  }
  entryBox.appendChild(bookmarkSection);

  var visitEntryWrapper = entryBox.appendChild(document.createElement('div'));
  if (addTitleFavicon || this.blockedVisit)
    visitEntryWrapper.classList.add('visit-entry');
  if (this.blockedVisit) {
    visitEntryWrapper.classList.add('blocked-indicator');
    visitEntryWrapper.appendChild(this.getVisitAttemptDOM_());
  } else {
    visitEntryWrapper.appendChild(this.getTitleDOM_(isSearchResult));
    if (addTitleFavicon)
      this.addFaviconToElement_(visitEntryWrapper);
    visitEntryWrapper.appendChild(domain);
  }

  if (isMobileVersion()) {
    var removeButton = createElementWithClassName('button', 'remove-entry');
    removeButton.setAttribute('aria-label',
                              loadTimeData.getString('removeFromHistory'));
    removeButton.classList.add('custom-appearance');
    removeButton.addEventListener('click', function(e) {
      self.removeFromHistory();
      e.stopPropagation();
      e.preventDefault();
    });
    entryBox.appendChild(removeButton);

    // Support clicking anywhere inside the entry box.
    entryBox.addEventListener('click', function(e) {
      e.currentTarget.querySelector('a').click();
    });
  } else {
    var dropDown = createElementWithClassName('button', 'drop-down');
    dropDown.value = 'Open action menu';
    dropDown.title = loadTimeData.getString('actionMenuDescription');
    dropDown.setAttribute('menu', '#action-menu');
    dropDown.setAttribute('aria-haspopup', 'true');
    cr.ui.decorate(dropDown, MenuButton);

    dropDown.addEventListener('mousedown', setActiveVisit);
    dropDown.addEventListener('focus', setActiveVisit);

    // Prevent clicks on the drop down from affecting the checkbox.  We need to
    // call blur() explicitly because preventDefault() cancels any focus
    // handling.
    dropDown.addEventListener('click', function(e) {
      e.preventDefault();
      document.activeElement.blur();
    });
    entryBox.appendChild(dropDown);
  }

  // Let the entryBox be styled appropriately when it contains keyboard focus.
  entryBox.addEventListener('focus', function() {
    this.classList.add('contains-focus');
  }, true);
  entryBox.addEventListener('blur', function() {
    this.classList.remove('contains-focus');
  }, true);

  var entryBoxContainer =
      createElementWithClassName('div', 'entry-box-container');
  node.appendChild(entryBoxContainer);
  entryBoxContainer.appendChild(entryBox);

  if (isSearchResult || useMonthDate) {
    // Show the day instead of the time.
    time.appendChild(document.createTextNode(this.dateShort));
  } else {
    time.appendChild(document.createTextNode(this.dateTimeOfDay));
  }

  this.domNode_ = node;
  node.visit = this;

  return node;
};

/**
 * Remove this visit from the history.
 */
Visit.prototype.removeFromHistory = function() {
  recordUmaAction('HistoryPage_EntryMenuRemoveFromHistory');
  var self = this;
  this.model_.removeVisitsFromHistory([this], function() {
    removeEntryFromView(self.domNode_);
  });
};

// Visit, private: ------------------------------------------------------------

/**
 * Add child text nodes to a node such that occurrences of the specified text is
 * highlighted.
 * @param {Node} node The node under which new text nodes will be made as
 *     children.
 * @param {string} content Text to be added beneath |node| as one or more
 *     text nodes.
 * @param {string} highlightText Occurences of this text inside |content| will
 *     be highlighted.
 * @private
 */
Visit.prototype.addHighlightedText_ = function(node, content, highlightText) {
  var i = 0;
  if (highlightText) {
    var re = new RegExp(Visit.pregQuote_(highlightText), 'gim');
    var match;
    while (match = re.exec(content)) {
      if (match.index > i)
        node.appendChild(document.createTextNode(content.slice(i,
                                                               match.index)));
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = content.substring(match.index, i);
      node.appendChild(b);
    }
  }
  if (i < content.length)
    node.appendChild(document.createTextNode(content.slice(i)));
};

/**
 * Returns the DOM element containing a link on the title of the URL for the
 * current visit.
 * @param {boolean} isSearchResult Whether or not the entry is a search result.
 * @return {Element} DOM representation for the title block.
 * @private
 */
Visit.prototype.getTitleDOM_ = function(isSearchResult) {
  var node = createElementWithClassName('div', 'title');
  var link = document.createElement('a');
  link.href = this.url_;
  link.id = 'id-' + this.id_;
  link.target = '_top';
  var integerId = parseInt(this.id_, 10);
  link.addEventListener('click', function() {
    recordUmaAction('HistoryPage_EntryLinkClick');
    // Record the ID of the entry to signify how many entries are above this
    // link on the page.
    recordUmaHistogram('HistoryPage.ClickPosition',
                       UMA_MAX_BUCKET_VALUE,
                       integerId);
    if (integerId <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      recordUmaHistogram('HistoryPage.ClickPositionSubset',
                         UMA_MAX_SUBSET_BUCKET_VALUE,
                         integerId);
    }
  });
  link.addEventListener('contextmenu', function() {
    recordUmaAction('HistoryPage_EntryLinkRightClick');
  });

  if (isSearchResult) {
    link.addEventListener('click', function() {
      recordUmaAction('HistoryPage_SearchResultClick');
    });
  }

  // Add a tooltip, since it might be ellipsized.
  // TODO(dubroy): Find a way to show the tooltip only when necessary.
  link.title = this.title_;

  this.addHighlightedText_(link, this.title_, this.model_.getSearchText());
  node.appendChild(link);

  return node;
};

/**
 * Returns the DOM element containing the text for a blocked visit attempt.
 * @return {Element} DOM representation of the visit attempt.
 * @private
 */
Visit.prototype.getVisitAttemptDOM_ = function() {
  var node = createElementWithClassName('div', 'title');
  node.innerHTML = loadTimeData.getStringF('blockedVisitText',
                                           this.url_,
                                           this.id_,
                                           this.domain_);
  return node;
};

/**
 * Set the favicon for an element.
 * @param {Element} el The DOM element to which to add the icon.
 * @private
 */
Visit.prototype.addFaviconToElement_ = function(el) {
  var url = isMobileVersion() ?
      getFaviconImageSet(this.url_, 32, 'touch-icon') :
      getFaviconImageSet(this.url_);
  el.style.backgroundImage = url;
};

/**
 * Launch a search for more history entries from the same domain.
 * @private
 */
Visit.prototype.showMoreFromSite_ = function() {
  recordUmaAction('HistoryPage_EntryMenuShowMoreFromSite');
  historyView.setSearch(this.domain_);
  $('search-field').focus();
};

// Visit, private, static: ----------------------------------------------------

/**
 * Quote a string so it can be used in a regular expression.
 * @param {string} str The source string.
 * @return {string} The escaped string.
 * @private
 */
Visit.pregQuote_ = function(str) {
  return str.replace(/([\\\.\+\*\?\[\^\]\$\(\)\{\}\=\!\<\>\|\:])/g, '\\$1');
};

///////////////////////////////////////////////////////////////////////////////
// HistoryModel:

/**
 * Global container for history data. Future optimizations might include
 * allowing the creation of a HistoryModel for each search string, allowing
 * quick flips back and forth between results.
 *
 * The history model is based around pages, and only fetching the data to
 * fill the currently requested page. This is somewhat dependent on the view,
 * and so future work may wish to change history model to operate on
 * timeframe (day or week) based containers.
 *
 * @constructor
 */
function HistoryModel() {
  this.clearModel_();
}

// HistoryModel, Public: ------------------------------------------------------

/** @enum {number} */
HistoryModel.Range = {
  ALL_TIME: 0,
  WEEK: 1,
  MONTH: 2
};

/**
 * Sets our current view that is called when the history model changes.
 * @param {HistoryView} view The view to set our current view to.
 */
HistoryModel.prototype.setView = function(view) {
  this.view_ = view;
};

/**
 * Reload our model with the current parameters.
 */
HistoryModel.prototype.reload = function() {
  // Save user-visible state, clear the model, and restore the state.
  var search = this.searchText_;
  var page = this.requestedPage_;
  var range = this.rangeInDays_;
  var offset = this.offset_;
  var groupByDomain = this.groupByDomain_;

  this.clearModel_();
  this.searchText_ = search;
  this.requestedPage_ = page;
  this.rangeInDays_ = range;
  this.offset_ = offset;
  this.groupByDomain_ = groupByDomain;
  this.queryHistory_();
};

/**
 * @return {string} The current search text.
 */
HistoryModel.prototype.getSearchText = function() {
  return this.searchText_;
};

/**
 * Tell the model that the view will want to see the current page. When
 * the data becomes available, the model will call the view back.
 * @param {number} page The page we want to view.
 */
HistoryModel.prototype.requestPage = function(page) {
  this.requestedPage_ = page;
  this.updateSearch_();
};

/**
 * Receiver for history query.
 * @param {Object} info An object containing information about the query.
 * @param {Array} results A list of results.
 */
HistoryModel.prototype.addResults = function(info, results) {
  // If no requests are in flight then this was an old request so we drop the
  // results. Double check the search term as well.
  if (!this.inFlight_ || info.term != this.searchText_)
    return;

  $('loading-spinner').hidden = true;
  this.inFlight_ = false;
  this.isQueryFinished_ = info.finished;
  this.queryStartTime = info.queryStartTime;
  this.queryEndTime = info.queryEndTime;

  var lastVisit = this.visits_.slice(-1)[0];
  var lastDay = lastVisit ? lastVisit.dateRelativeDay : null;

  for (var i = 0, result; result = results[i]; i++) {
    var thisDay = result.dateRelativeDay;
    var isSameDay = lastDay == thisDay;
    this.visits_.push(new Visit(result, isSameDay, this));
    lastDay = thisDay;
  }

  if (loadTimeData.getBoolean('isUserSignedIn')) {
    var message = loadTimeData.getString(
        info.hasSyncedResults ? 'hasSyncedResults' : 'noSyncedResults');
    this.view_.showNotification(message);
  }

  this.updateSearch_();
};

/**
 * @return {number} The number of visits in the model.
 */
HistoryModel.prototype.getSize = function() {
  return this.visits_.length;
};

/**
 * Get a list of visits between specified index positions.
 * @param {number} start The start index.
 * @param {number} end The end index.
 * @return {Array.<Visit>} A list of visits.
 */
HistoryModel.prototype.getNumberedRange = function(start, end) {
  return this.visits_.slice(start, end);
};

/**
 * Return true if there are more results beyond the current page.
 * @return {boolean} true if the there are more results, otherwise false.
 */
HistoryModel.prototype.hasMoreResults = function() {
  return this.haveDataForPage_(this.requestedPage_ + 1) ||
      !this.isQueryFinished_;
};

/**
 * Removes a list of visits from the history, and calls |callback| when the
 * removal has successfully completed.
 * @param {Array<Visit>} visits The visits to remove.
 * @param {Function} callback The function to call after removal succeeds.
 */
HistoryModel.prototype.removeVisitsFromHistory = function(visits, callback) {
  var toBeRemoved = [];
  for (var i = 0; i < visits.length; i++) {
    toBeRemoved.push({
      url: visits[i].url_,
      timestamps: visits[i].allTimestamps
    });
  }
  chrome.send('removeVisits', toBeRemoved);
  this.deleteCompleteCallback_ = callback;
};

/**
 * Called when visits have been succesfully removed from the history.
 */
HistoryModel.prototype.deleteComplete = function() {
  // Call the callback, with 'this' undefined inside the callback.
  this.deleteCompleteCallback_.call();
  this.deleteCompleteCallback_ = null;
};

// Getter and setter for HistoryModel.rangeInDays_.
Object.defineProperty(HistoryModel.prototype, 'rangeInDays', {
  get: function() {
    return this.rangeInDays_;
  },
  set: function(range) {
    this.rangeInDays_ = range;
  }
});

/**
 * Getter and setter for HistoryModel.offset_. The offset moves the current
 * query 'window' |range| days behind. As such for range set to WEEK an offset
 * of 0 refers to the last 7 days, an offset of 1 refers to the 7 day period
 * that ended 7 days ago, etc. For MONTH an offset of 0 refers to the current
 * calendar month, 1 to the previous one, etc.
 */
Object.defineProperty(HistoryModel.prototype, 'offset', {
  get: function() {
    return this.offset_;
  },
  set: function(offset) {
    this.offset_ = offset;
  }
});

// Setter for HistoryModel.requestedPage_.
Object.defineProperty(HistoryModel.prototype, 'requestedPage', {
  set: function(page) {
    this.requestedPage_ = page;
  }
});

// HistoryModel, Private: -----------------------------------------------------

/**
 * Clear the history model.
 * @private
 */
HistoryModel.prototype.clearModel_ = function() {
  this.inFlight_ = false;  // Whether a query is inflight.
  this.searchText_ = '';
  // Whether this user is a managed user.
  this.isManagedProfile = loadTimeData.getBoolean('isManagedProfile');
  this.deletingHistoryAllowed = loadTimeData.getBoolean('allowDeletingHistory');

  // Only create checkboxes for editing entries if they can be used either to
  // delete an entry or to block/allow it.
  this.editingEntriesAllowed = this.deletingHistoryAllowed;

  // Flag to show that the results are grouped by domain or not.
  this.groupByDomain_ = false;

  this.visits_ = [];  // Date-sorted list of visits (most recent first).
  this.nextVisitId_ = 0;
  selectionAnchor = -1;

  // The page that the view wants to see - we only fetch slightly past this
  // point. If the view requests a page that we don't have data for, we try
  // to fetch it and call back when we're done.
  this.requestedPage_ = 0;

  // The range of history to view or search over.
  this.rangeInDays_ = HistoryModel.Range.ALL_TIME;

  // Skip |offset_| * weeks/months from the begining.
  this.offset_ = 0;

  // Keeps track of whether or not there are more results available than are
  // currently held in |this.visits_|.
  this.isQueryFinished_ = false;

  if (this.view_)
    this.view_.clear_();
};

/**
 * Figure out if we need to do more queries to fill the currently requested
 * page. If we think we can fill the page, call the view and let it know
 * we're ready to show something. This only applies to the daily time-based
 * view.
 * @private
 */
HistoryModel.prototype.updateSearch_ = function() {
  var doneLoading = this.rangeInDays_ != HistoryModel.Range.ALL_TIME ||
                    this.isQueryFinished_ ||
                    this.canFillPage_(this.requestedPage_);

  // Try to fetch more results if more results can arrive and the page is not
  // full.
  if (!doneLoading && !this.inFlight_)
    this.queryHistory_();

  // Show the result or a message if no results were returned.
  this.view_.onModelReady(doneLoading);
};

/**
 * Query for history, either for a search or time-based browsing.
 * @private
 */
HistoryModel.prototype.queryHistory_ = function() {
  var maxResults =
      (this.rangeInDays_ == HistoryModel.Range.ALL_TIME) ? RESULTS_PER_PAGE : 0;

  // If there are already some visits, pick up the previous query where it
  // left off.
  var lastVisit = this.visits_.slice(-1)[0];
  var endTime = lastVisit ? lastVisit.date.getTime() : 0;

  $('loading-spinner').hidden = false;
  this.inFlight_ = true;
  chrome.send('queryHistory',
      [this.searchText_, this.offset_, this.rangeInDays_, endTime, maxResults]);
};

/**
 * Check to see if we have data for the given page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have any data for the given page.
 * @private
 */
HistoryModel.prototype.haveDataForPage_ = function(page) {
  return page * RESULTS_PER_PAGE < this.getSize();
};

/**
 * Check to see if we have data to fill the given page.
 * @param {number} page The page number.
 * @return {boolean} Whether we have data to fill the page.
 * @private
 */
HistoryModel.prototype.canFillPage_ = function(page) {
  return ((page + 1) * RESULTS_PER_PAGE <= this.getSize());
};

/**
 * Enables or disables grouping by domain.
 * @param {boolean} groupByDomain New groupByDomain_ value.
 */
HistoryModel.prototype.setGroupByDomain = function(groupByDomain) {
  this.groupByDomain_ = groupByDomain;
  this.offset_ = 0;
};

/**
 * Gets whether we are grouped by domain.
 * @return {boolean} Whether the results are grouped by domain.
 */
HistoryModel.prototype.getGroupByDomain = function() {
  return this.groupByDomain_;
};

///////////////////////////////////////////////////////////////////////////////
// HistoryView:

/**
 * Functions and state for populating the page with HTML. This should one-day
 * contain the view and use event handlers, rather than pushing HTML out and
 * getting called externally.
 * @param {HistoryModel} model The model backing this view.
 * @constructor
 */
function HistoryView(model) {
  this.editButtonTd_ = $('edit-button');
  this.editingControlsDiv_ = $('editing-controls');
  this.resultDiv_ = $('results-display');
  this.pageDiv_ = $('results-pagination');
  this.model_ = model;
  this.pageIndex_ = 0;
  this.lastDisplayed_ = [];

  this.model_.setView(this);

  this.currentVisits_ = [];

  // If there is no search button, use the search button label as placeholder
  // text in the search field.
  if ($('search-button').offsetWidth == 0)
    $('search-field').placeholder = $('search-button').value;

  var self = this;

  $('clear-browsing-data').addEventListener('click', openClearBrowsingData);
  $('remove-selected').addEventListener('click', removeItems);

  // Add handlers for the page navigation buttons at the bottom.
  $('newest-button').addEventListener('click', function() {
    recordUmaAction('HistoryPage_NewestHistoryClick');
    self.setPage(0);
  });
  $('newer-button').addEventListener('click', function() {
    recordUmaAction('HistoryPage_NewerHistoryClick');
    self.setPage(self.pageIndex_ - 1);
  });
  $('older-button').addEventListener('click', function() {
    recordUmaAction('HistoryPage_OlderHistoryClick');
    self.setPage(self.pageIndex_ + 1);
  });

  var handleRangeChange = function(e) {
    // Update the results and save the last state.
    self.setRangeInDays(parseInt(e.target.value, 10));
  };

  // Add handlers for the range options.
  $('timeframe-filter-all').addEventListener('change', handleRangeChange);
  $('timeframe-filter-week').addEventListener('change', handleRangeChange);
  $('timeframe-filter-month').addEventListener('change', handleRangeChange);

  $('range-previous').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(self.pageIndex_ + 1);
    else
      self.setOffset(self.getOffset() + 1);
  });
  $('range-next').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(self.pageIndex_ - 1);
    else
      self.setOffset(self.getOffset() - 1);
  });
  $('range-today').addEventListener('click', function(e) {
    if (self.getRangeInDays() == HistoryModel.Range.ALL_TIME)
      self.setPage(0);
    else
      self.setOffset(0);
  });
}

// HistoryView, public: -------------------------------------------------------
/**
 * Do a search on a specific term.
 * @param {string} term The string to search for.
 */
HistoryView.prototype.setSearch = function(term) {
  window.scrollTo(0, 0);
  this.setPageState(term, 0, this.getRangeInDays(), this.getOffset());
};

/**
 * Reload the current view.
 */
HistoryView.prototype.reload = function() {
  this.model_.reload();
  this.updateSelectionEditButtons();
  this.updateRangeButtons_();
};

/**
 * Sets all the parameters for the history page and then reloads the view to
 * update the results.
 * @param {string} searchText The search string to set.
 * @param {number} page The page to be viewed.
 * @param {HistoryModel.Range} range The range to view or search over.
 * @param {number} offset Set the begining of the query to the specific offset.
 */
HistoryView.prototype.setPageState = function(searchText, page, range, offset) {
  this.clear_();
  this.model_.searchText_ = searchText;
  this.pageIndex_ = page;
  this.model_.requestedPage_ = page;
  this.model_.rangeInDays_ = range;
  this.model_.groupByDomain_ = false;
  if (range != HistoryModel.Range.ALL_TIME)
    this.model_.groupByDomain_ = true;
  this.model_.offset_ = offset;
  this.reload();
  pageState.setUIState(this.model_.getSearchText(),
                       this.pageIndex_,
                       this.getRangeInDays(),
                       this.getOffset());
};

/**
 * Switch to a specified page.
 * @param {number} page The page we wish to view.
 */
HistoryView.prototype.setPage = function(page) {
  // TODO(sergiu): Move this function to setPageState as well and see why one
  // of the tests fails when using setPageState.
  this.clear_();
  this.pageIndex_ = parseInt(page, 10);
  window.scrollTo(0, 0);
  this.model_.requestPage(page);
  pageState.setUIState(this.model_.getSearchText(),
                       this.pageIndex_,
                       this.getRangeInDays(),
                       this.getOffset());
};

/**
 * @return {number} The page number being viewed.
 */
HistoryView.prototype.getPage = function() {
  return this.pageIndex_;
};

/**
 * Set the current range for grouped results.
 * @param {string} range The number of days to which the range should be set.
 */
HistoryView.prototype.setRangeInDays = function(range) {
  // Set the range, offset and reset the page.
  this.setPageState(this.model_.getSearchText(), 0, range, 0);
};

/**
 * Get the current range in days.
 * @return {number} Current range in days from the model.
 */
HistoryView.prototype.getRangeInDays = function() {
  return this.model_.rangeInDays;
};

/**
 * Set the current offset for grouped results.
 * @param {number} offset Offset to set.
 */
HistoryView.prototype.setOffset = function(offset) {
  // If there is another query already in flight wait for that to complete.
  if (this.model_.inFlight_)
    return;
  this.setPageState(this.model_.getSearchText(),
                    this.pageIndex_,
                    this.getRangeInDays(),
                    offset);
};

/**
 * Get the current offset.
 * @return {number} Current offset from the model.
 */
HistoryView.prototype.getOffset = function() {
  return this.model_.offset;
};

/**
 * Callback for the history model to let it know that it has data ready for us
 * to view.
 * @param {boolean} doneLoading Whether the current request is complete.
 */
HistoryView.prototype.onModelReady = function(doneLoading) {
  this.displayResults_(doneLoading);

  // Allow custom styling based on whether there are any results on the page.
  // To make this easier, add a class to the body if there are any results.
  if (this.model_.visits_.length)
    document.body.classList.add('has-results');
  else
    document.body.classList.remove('has-results');

  this.updateNavBar_();

  if (isMobileVersion()) {
    // Hide the search field if it is empty and there are no results.
    var hasResults = this.model_.visits_.length > 0;
    var isSearch = this.model_.getSearchText().length > 0;
    $('search-field').hidden = !(hasResults || isSearch);
  }
};

/**
 * Enables or disables the buttons that control editing entries depending on
 * whether there are any checked boxes.
 */
HistoryView.prototype.updateSelectionEditButtons = function() {
  if (loadTimeData.getBoolean('allowDeletingHistory')) {
    var anyChecked = document.querySelector('.entry input:checked') != null;
    $('remove-selected').disabled = !anyChecked;
  } else {
    $('remove-selected').disabled = true;
  }
};

/**
 * Shows the notification bar at the top of the page with |innerHTML| as its
 * content.
 * @param {string} innerHTML The HTML content of the warning.
 * @param {boolean} isWarning If true, style the notification as a warning.
 */
HistoryView.prototype.showNotification = function(innerHTML, isWarning) {
  var bar = $('notification-bar');
  bar.innerHTML = innerHTML;
  bar.hidden = false;
  if (isWarning)
    bar.classList.add('warning');
  else
    bar.classList.remove('warning');

  // Make sure that any links in the HTML are targeting the top level.
  var links = bar.querySelectorAll('a');
  for (var i = 0; i < links.length; i++)
    links[i].target = '_top';

  this.positionNotificationBar();
};

/**
 * Adjusts the position of the notification bar based on the size of the page.
 */
HistoryView.prototype.positionNotificationBar = function() {
  var bar = $('notification-bar');

  // If the bar does not fit beside the editing controls, put it into the
  // overflow state.
  if (bar.getBoundingClientRect().top >=
      $('editing-controls').getBoundingClientRect().bottom) {
    bar.classList.add('alone');
  } else {
    bar.classList.remove('alone');
  }
};

// HistoryView, private: ------------------------------------------------------

/**
 * Clear the results in the view.  Since we add results piecemeal, we need
 * to clear them out when we switch to a new page or reload.
 * @private
 */
HistoryView.prototype.clear_ = function() {
  var alertOverlay = $('alertOverlay');
  if (alertOverlay && alertOverlay.classList.contains('showing'))
    hideConfirmationOverlay();

  this.resultDiv_.textContent = '';

  this.currentVisits_.forEach(function(visit) {
    visit.isRendered = false;
  });
  this.currentVisits_ = [];

  document.body.classList.remove('has-results');
};

/**
 * Record that the given visit has been rendered.
 * @param {Visit} visit The visit that was rendered.
 * @private
 */
HistoryView.prototype.setVisitRendered_ = function(visit) {
  visit.isRendered = true;
  this.currentVisits_.push(visit);
};

/**
 * Generates and adds the grouped visits DOM for a certain domain. This
 * includes the clickable arrow and domain name and the visit entries for
 * that domain.
 * @param {Element} results DOM object to which to add the elements.
 * @param {string} domain Current domain name.
 * @param {Array} domainVisits Array of visits for this domain.
 * @private
 */
HistoryView.prototype.getGroupedVisitsDOM_ = function(
    results, domain, domainVisits) {
  // Add a new domain entry.
  var siteResults = results.appendChild(
      createElementWithClassName('li', 'site-entry'));

  // Make a wrapper that will contain the arrow, the favicon and the domain.
  var siteDomainWrapper = siteResults.appendChild(
      createElementWithClassName('div', 'site-domain-wrapper'));

  if (this.model_.editingEntriesAllowed) {
    var siteDomainCheckbox =
        createElementWithClassName('input', 'domain-checkbox');

    siteDomainCheckbox.type = 'checkbox';
    siteDomainCheckbox.addEventListener('click', domainCheckboxClicked);
    siteDomainCheckbox.domain_ = domain;

    siteDomainWrapper.appendChild(siteDomainCheckbox);
  }

  var siteArrow = siteDomainWrapper.appendChild(
      createElementWithClassName('div', 'site-domain-arrow collapse'));
  var siteDomain = siteDomainWrapper.appendChild(
      createElementWithClassName('div', 'site-domain'));
  var siteDomainLink = siteDomain.appendChild(
      createElementWithClassName('button', 'link-button'));
  siteDomainLink.addEventListener('click', function(e) { e.preventDefault(); });
  siteDomainLink.textContent = domain;
  var numberOfVisits = createElementWithClassName('span', 'number-visits');
  var domainElement = document.createElement('span');

  numberOfVisits.textContent = loadTimeData.getStringF('numberVisits',
                                                       domainVisits.length);
  siteDomain.appendChild(numberOfVisits);

  domainVisits[0].addFaviconToElement_(siteDomain);

  siteDomainWrapper.addEventListener('click', toggleHandler);

  if (this.model_.isManagedProfile) {
    siteDomainWrapper.appendChild(
        getManagedStatusDOM(domainVisits[0].hostFilteringBehavior));
  }

  siteResults.appendChild(siteDomainWrapper);
  var resultsList = siteResults.appendChild(
      createElementWithClassName('ol', 'site-results'));
  resultsList.classList.add('grouped');

  // Collapse until it gets toggled.
  resultsList.style.height = 0;

  // Add the results for each of the domain.
  var isMonthGroupedResult = this.getRangeInDays() == HistoryModel.Range.MONTH;
  for (var j = 0, visit; visit = domainVisits[j]; j++) {
    resultsList.appendChild(visit.getResultDOM({
      useMonthDate: isMonthGroupedResult
    }));
    this.setVisitRendered_(visit);
  }
};

/**
 * Enables or disables the time range buttons.
 * @private
 */
HistoryView.prototype.updateRangeButtons_ = function() {
  // The enabled state for the previous, today and next buttons.
  var previousState = false;
  var todayState = false;
  var nextState = false;
  var usePage = (this.getRangeInDays() == HistoryModel.Range.ALL_TIME);

  // Use pagination for most recent visits, offset otherwise.
  // TODO(sergiu): Maybe send just one variable in the future.
  if (usePage) {
    if (this.getPage() != 0) {
      nextState = true;
      todayState = true;
    }
    previousState = this.model_.hasMoreResults();
  } else {
    if (this.getOffset() != 0) {
      nextState = true;
      todayState = true;
    }
    previousState = !this.model_.isQueryFinished_;
  }

  $('range-previous').disabled = !previousState;
  $('range-today').disabled = !todayState;
  $('range-next').disabled = !nextState;
};

/**
 * Groups visits by domain, sorting them by the number of visits.
 * @param {Array} visits Visits received from the query results.
 * @param {Element} results Object where the results are added to.
 * @private
 */
HistoryView.prototype.groupVisitsByDomain_ = function(visits, results) {
  var visitsByDomain = {};
  var domains = [];

  // Group the visits into a dictionary and generate a list of domains.
  for (var i = 0, visit; visit = visits[i]; i++) {
    var domain = visit.domain_;
    if (!visitsByDomain[domain]) {
      visitsByDomain[domain] = [];
      domains.push(domain);
    }
    visitsByDomain[domain].push(visit);
  }
  var sortByVisits = function(a, b) {
    return visitsByDomain[b].length - visitsByDomain[a].length;
  };
  domains.sort(sortByVisits);

  for (var i = 0; i < domains.length; ++i) {
    var domain = domains[i];
    this.getGroupedVisitsDOM_(results, domain, visitsByDomain[domain]);
  }
};

/**
 * Adds the results for a month.
 * @param {Array} visits Visits returned by the query.
 * @param {Element} parentElement Element to which to add the results to.
 * @private
 */
HistoryView.prototype.addMonthResults_ = function(visits, parentElement) {
  if (visits.length == 0)
    return;

  var monthResults = parentElement.appendChild(
      createElementWithClassName('ol', 'month-results'));
  // Don't add checkboxes if entries can not be edited.
  if (!this.model_.editingEntriesAllowed)
    monthResults.classList.add('no-checkboxes');

  this.groupVisitsByDomain_(visits, monthResults);
};

/**
 * Adds the results for a certain day. This includes a title with the day of
 * the results and the results themselves, grouped or not.
 * @param {Array} visits Visits returned by the query.
 * @param {Element} parentElement Element to which to add the results to.
 * @private
 */
HistoryView.prototype.addDayResults_ = function(visits, parentElement) {
  if (visits.length == 0)
    return;

  var firstVisit = visits[0];
  var day = parentElement.appendChild(createElementWithClassName('h3', 'day'));
  day.appendChild(document.createTextNode(firstVisit.dateRelativeDay));
  if (firstVisit.continued) {
    day.appendChild(document.createTextNode(' ' +
                                            loadTimeData.getString('cont')));
  }
  var dayResults = parentElement.appendChild(
      createElementWithClassName('ol', 'day-results'));

  // Don't add checkboxes if entries can not be edited.
  if (!this.model_.editingEntriesAllowed)
    dayResults.classList.add('no-checkboxes');

  if (this.model_.getGroupByDomain()) {
    this.groupVisitsByDomain_(visits, dayResults);
  } else {
    var lastTime;

    for (var i = 0, visit; visit = visits[i]; i++) {
      // If enough time has passed between visits, indicate a gap in browsing.
      var thisTime = visit.date.getTime();
      if (lastTime && lastTime - thisTime > BROWSING_GAP_TIME)
        dayResults.appendChild(createElementWithClassName('li', 'gap'));

      // Insert the visit into the DOM.
      dayResults.appendChild(visit.getResultDOM({ addTitleFavicon: true }));
      this.setVisitRendered_(visit);

      lastTime = thisTime;
    }
  }
};

/**
 * Adds the text that shows the current interval, used for week and month
 * results.
 * @param {Element} resultsFragment The element to which the interval will be
 *     added to.
 * @private
 */
HistoryView.prototype.addTimeframeInterval_ = function(resultsFragment) {
  if (this.getRangeInDays() == HistoryModel.Range.ALL_TIME)
    return;

  // If this is a time range result add some text that shows what is the
  // time range for the results the user is viewing.
  var timeFrame = resultsFragment.appendChild(
      createElementWithClassName('h2', 'timeframe'));
  // TODO(sergiu): Figure the best way to show this for the first day of
  // the month.
  timeFrame.appendChild(document.createTextNode(loadTimeData.getStringF(
      'historyInterval',
      this.model_.queryStartTime,
      this.model_.queryEndTime)));
};

/**
 * Update the page with results.
 * @param {boolean} doneLoading Whether the current request is complete.
 * @private
 */
HistoryView.prototype.displayResults_ = function(doneLoading) {
  // Either show a page of results received for the all time results or all the
  // received results for the weekly and monthly view.
  var results = this.model_.visits_;
  if (this.getRangeInDays() == HistoryModel.Range.ALL_TIME) {
    var rangeStart = this.pageIndex_ * RESULTS_PER_PAGE;
    var rangeEnd = rangeStart + RESULTS_PER_PAGE;
    results = this.model_.getNumberedRange(rangeStart, rangeEnd);
  }
  var searchText = this.model_.getSearchText();
  var groupByDomain = this.model_.getGroupByDomain();

  if (searchText) {
    // Add a header for the search results, if there isn't already one.
    if (!this.resultDiv_.querySelector('h3')) {
      var header = document.createElement('h3');
      header.textContent = loadTimeData.getStringF('searchResultsFor',
                                                   searchText);
      this.resultDiv_.appendChild(header);
    }

    this.addTimeframeInterval_(this.resultDiv_);

    var searchResults = createElementWithClassName('ol', 'search-results');

    // Don't add checkboxes if entries can not be edited.
    if (!this.model_.editingEntriesAllowed)
      searchResults.classList.add('no-checkboxes');

    if (results.length == 0 && doneLoading) {
      var noSearchResults = searchResults.appendChild(
          createElementWithClassName('div', 'no-results-message'));
      noSearchResults.textContent = loadTimeData.getString('noSearchResults');
    } else {
      for (var i = 0, visit; visit = results[i]; i++) {
        if (!visit.isRendered) {
          searchResults.appendChild(visit.getResultDOM({
            isSearchResult: true,
            addTitleFavicon: true
          }));
          this.setVisitRendered_(visit);
        }
      }
    }
    this.resultDiv_.appendChild(searchResults);
  } else {
    var resultsFragment = document.createDocumentFragment();

    this.addTimeframeInterval_(resultsFragment);

    if (results.length == 0 && doneLoading) {
      var noResults = resultsFragment.appendChild(
          createElementWithClassName('div', 'no-results-message'));
      noResults.textContent = loadTimeData.getString('noResults');
      this.resultDiv_.appendChild(resultsFragment);
      return;
    }

    if (this.getRangeInDays() == HistoryModel.Range.MONTH &&
        groupByDomain) {
      // Group everything together in the month view.
      this.addMonthResults_(results, resultsFragment);
    } else {
      var dayStart = 0;
      var dayEnd = 0;
      // Go through all of the visits and process them in chunks of one day.
      while (dayEnd < results.length) {
        // Skip over the ones that are already rendered.
        while (dayStart < results.length && results[dayStart].isRendered)
          ++dayStart;
        var dayEnd = dayStart + 1;
        while (dayEnd < results.length && results[dayEnd].continued)
          ++dayEnd;

        this.addDayResults_(
            results.slice(dayStart, dayEnd), resultsFragment, groupByDomain);
      }
    }

    // Add all the days and their visits to the page.
    this.resultDiv_.appendChild(resultsFragment);
  }
  // After the results have been added to the DOM, determine the size of the
  // time column.
  this.setTimeColumnWidth_(this.resultDiv_);
};

/**
 * Update the visibility of the page navigation buttons.
 * @private
 */
HistoryView.prototype.updateNavBar_ = function() {
  this.updateRangeButtons_();

  // Managed users have the control bar on top, don't show it on the bottom
  // as well.
  if (!loadTimeData.getBoolean('isManagedProfile')) {
    $('newest-button').hidden = this.pageIndex_ == 0;
    $('newer-button').hidden = this.pageIndex_ == 0;
    $('older-button').hidden =
        this.model_.rangeInDays_ != HistoryModel.Range.ALL_TIME ||
        !this.model_.hasMoreResults();
  }
};

/**
 * Updates the visibility of the 'Clear browsing data' button.
 * Only used on mobile platforms.
 * @private
 */
HistoryView.prototype.updateClearBrowsingDataButton_ = function() {
  // Ideally, we should hide the 'Clear browsing data' button whenever the
  // soft keyboard is visible. This is not possible, so instead, hide the
  // button whenever the search field has focus.
  $('clear-browsing-data').hidden =
      (document.activeElement === $('search-field'));
};

/**
 * Dynamically sets the min-width of the time column for history entries.
 * This ensures that all entry times will have the same width, without
 * imposing a fixed width that may not be appropriate for some locales.
 * @private
 */
HistoryView.prototype.setTimeColumnWidth_ = function() {
  // Find the maximum width of all the time elements on the page.
  var times = this.resultDiv_.querySelectorAll('.entry .time');
  var widths = Array.prototype.map.call(times, function(el) {
    el.style.minWidth = '-webkit-min-content';
    var width = el.clientWidth;
    el.style.minWidth = '';

    // Add an extra pixel to prevent rounding errors from causing the text to
    // be ellipsized at certain zoom levels (see crbug.com/329779).
    return width + 1;
  });
  var maxWidth = widths.length ? Math.max.apply(null, widths) : 0;

  // Add a dynamic stylesheet to the page (or replace the existing one), to
  // ensure that all entry times have the same width.
  var styleEl = $('timeColumnStyle');
  if (!styleEl) {
    styleEl = document.head.appendChild(document.createElement('style'));
    styleEl.id = 'timeColumnStyle';
  }
  styleEl.textContent = '.entry .time { min-width: ' + maxWidth + 'px; }';
};

///////////////////////////////////////////////////////////////////////////////
// State object:
/**
 * An 'AJAX-history' implementation.
 * @param {HistoryModel} model The model we're representing.
 * @param {HistoryView} view The view we're representing.
 * @constructor
 */
function PageState(model, view) {
  // Enforce a singleton.
  if (PageState.instance) {
    return PageState.instance;
  }

  this.model = model;
  this.view = view;

  if (typeof this.checker_ != 'undefined' && this.checker_) {
    clearInterval(this.checker_);
  }

  // TODO(glen): Replace this with a bound method so we don't need
  //     public model and view.
  this.checker_ = window.setInterval(function(stateObj) {
    var hashData = stateObj.getHashData();
    var page = parseInt(hashData.page, 10);
    var range = parseInt(hashData.range, 10);
    var offset = parseInt(hashData.offset, 10);
    if (hashData.q != stateObj.model.getSearchText() ||
        page != stateObj.view.getPage() ||
        range != stateObj.model.rangeInDays ||
        offset != stateObj.model.offset) {
      stateObj.view.setPageState(hashData.q, page, range, offset);
    }
  }, 50, this);
}

/**
 * Holds the singleton instance.
 */
PageState.instance = null;

/**
 * @return {Object} An object containing parameters from our window hash.
 */
PageState.prototype.getHashData = function() {
  var result = {
    q: '',
    page: 0,
    grouped: false,
    range: 0,
    offset: 0
  };

  if (!window.location.hash)
    return result;

  var hashSplit = window.location.hash.substr(1).split('&');
  for (var i = 0; i < hashSplit.length; i++) {
    var pair = hashSplit[i].split('=');
    if (pair.length > 1) {
      result[pair[0]] = decodeURIComponent(pair[1].replace(/\+/g, ' '));
    }
  }

  return result;
};

/**
 * Set the hash to a specified state, this will create an entry in the
 * session history so the back button cycles through hash states, which
 * are then picked up by our listener.
 * @param {string} term The current search string.
 * @param {number} page The page currently being viewed.
 * @param {HistoryModel.Range} range The range to view or search over.
 * @param {number} offset Set the begining of the query to the specific offset.
 */
PageState.prototype.setUIState = function(term, page, range, offset) {
  // Make sure the form looks pretty.
  $('search-field').value = term;
  var hash = this.getHashData();
  if (hash.q != term || hash.page != page || hash.range != range ||
      hash.offset != offset) {
    window.location.hash = PageState.getHashString(term, page, range, offset);
  }
};

/**
 * Static method to get the hash string for a specified state
 * @param {string} term The current search string.
 * @param {number} page The page currently being viewed.
 * @param {HistoryModel.Range} range The range to view or search over.
 * @param {number} offset Set the begining of the query to the specific offset.
 * @return {string} The string to be used in a hash.
 */
PageState.getHashString = function(term, page, range, offset) {
  // Omit elements that are empty.
  var newHash = [];

  if (term)
    newHash.push('q=' + encodeURIComponent(term));

  if (page)
    newHash.push('page=' + page);

  if (range)
    newHash.push('range=' + range);

  if (offset)
    newHash.push('offset=' + offset);

  return newHash.join('&');
};

///////////////////////////////////////////////////////////////////////////////
// Document Functions:
/**
 * Window onload handler, sets up the page.
 */
function load() {
  uber.onContentFrameLoaded();

  var searchField = $('search-field');

  historyModel = new HistoryModel();
  historyView = new HistoryView(historyModel);
  pageState = new PageState(historyModel, historyView);

  // Create default view.
  var hashData = pageState.getHashData();
  var grouped = (hashData.grouped == 'true') || historyModel.getGroupByDomain();
  var page = parseInt(hashData.page, 10) || historyView.getPage();
  var range = parseInt(hashData.range, 10) || historyView.getRangeInDays();
  var offset = parseInt(hashData.offset, 10) || historyView.getOffset();
  historyView.setPageState(hashData.q, page, range, offset);

  if ($('overlay')) {
    cr.ui.overlay.setupOverlay($('overlay'));
    cr.ui.overlay.globalInitialization();
  }
  HistoryFocusManager.getInstance().initialize();

  var doSearch = function(e) {
    recordUmaAction('HistoryPage_Search');
    historyView.setSearch(searchField.value);

    if (isMobileVersion())
      searchField.blur();  // Dismiss the keyboard.
  };

  var mayRemoveVisits = loadTimeData.getBoolean('allowDeletingHistory');
  $('remove-visit').disabled = !mayRemoveVisits;

  if (mayRemoveVisits) {
    $('remove-visit').addEventListener('activate', function(e) {
      activeVisit.removeFromHistory();
      activeVisit = null;
    });
  }

  if (!loadTimeData.getBoolean('showDeleteVisitUI'))
    $('remove-visit').hidden = true;

  searchField.addEventListener('search', doSearch);
  $('search-button').addEventListener('click', doSearch);

  $('more-from-site').addEventListener('activate', function(e) {
    activeVisit.showMoreFromSite_();
    activeVisit = null;
  });

  // Only show the controls if the command line switch is activated.
  if (loadTimeData.getBoolean('groupByDomain') ||
      loadTimeData.getBoolean('isManagedProfile')) {
    // Hide the top container which has the "Clear browsing data" and "Remove
    // selected entries" buttons since they're unavailable in managed mode
    $('top-container').hidden = true;
    $('history-page').classList.add('big-topbar-page');
    $('filter-controls').hidden = false;
  }

  uber.setTitle(loadTimeData.getString('title'));

  // Adjust the position of the notification bar when the window size changes.
  window.addEventListener('resize',
      historyView.positionNotificationBar.bind(historyView));

  cr.ui.FocusManager.disableMouseFocusOnButtons();

  if (isMobileVersion()) {
    // Move the search box out of the header.
    var resultsDisplay = $('results-display');
    resultsDisplay.parentNode.insertBefore($('search-field'), resultsDisplay);

    window.addEventListener(
        'resize', historyView.updateClearBrowsingDataButton_);

    // When the search field loses focus, add a delay before updating the
    // visibility, otherwise the button will flash on the screen before the
    // keyboard animates away.
    searchField.addEventListener('blur', function() {
      setTimeout(historyView.updateClearBrowsingDataButton_, 250);
    });

    // Move the button to the bottom of the page.
    $('history-page').appendChild($('clear-browsing-data'));
  } else {
    window.addEventListener('message', function(e) {
      if (e.data.method == 'frameSelected')
        searchField.focus();
    });
    searchField.focus();
  }

<if expr="is_ios">
  function checkKeyboardVisibility() {
    // Figure out the real height based on the orientation, becauase
    // screen.width and screen.height don't update after rotation.
    var screenHeight = window.orientation % 180 ? screen.width : screen.height;

    // Assume that the keyboard is visible if more than 30% of the screen is
    // taken up by window chrome.
    var isKeyboardVisible = (window.innerHeight / screenHeight) < 0.7;

    document.body.classList.toggle('ios-keyboard-visible', isKeyboardVisible);
  }
  window.addEventListener('orientationchange', checkKeyboardVisibility);
  window.addEventListener('resize', checkKeyboardVisibility);
</if> /* is_ios */
}

/**
 * Updates the managed filter status labels of a host/URL entry to the current
 * value.
 * @param {Element} statusElement The div which contains the status labels.
 * @param {ManagedModeFilteringBehavior} newStatus The filter status of the
 *     current domain/URL.
 */
function updateHostStatus(statusElement, newStatus) {
  var filteringBehaviorDiv =
      statusElement.querySelector('.filtering-behavior');
  // Reset to the base class first, then add modifier classes if needed.
  filteringBehaviorDiv.className = 'filtering-behavior';
  if (newStatus == ManagedModeFilteringBehavior.BLOCK) {
    filteringBehaviorDiv.textContent =
        loadTimeData.getString('filterBlocked');
    filteringBehaviorDiv.classList.add('filter-blocked');
  } else {
    filteringBehaviorDiv.textContent = '';
  }
}

/**
 * Click handler for the 'Clear browsing data' dialog.
 * @param {Event} e The click event.
 */
function openClearBrowsingData(e) {
  recordUmaAction('HistoryPage_InitClearBrowsingData');
  chrome.send('clearBrowsingData');
}

/**
 * Shows the dialog for the user to confirm removal of selected history entries.
 */
function showConfirmationOverlay() {
  $('alertOverlay').classList.add('showing');
  $('overlay').hidden = false;
  uber.invokeMethodOnParent('beginInterceptingEvents');
}

/**
 * Hides the confirmation overlay used to confirm selected history entries.
 */
function hideConfirmationOverlay() {
  $('alertOverlay').classList.remove('showing');
  $('overlay').hidden = true;
  uber.invokeMethodOnParent('stopInterceptingEvents');
}

/**
 * Shows the confirmation alert for history deletions and permits browser tests
 * to override the dialog.
 * @param {function=} okCallback A function to be called when the user presses
 *     the ok button.
 * @param {function=} cancelCallback A function to be called when the user
 *     presses the cancel button.
 */
function confirmDeletion(okCallback, cancelCallback) {
  alertOverlay.setValues(
      loadTimeData.getString('removeSelected'),
      loadTimeData.getString('deleteWarning'),
      loadTimeData.getString('cancel'),
      loadTimeData.getString('deleteConfirm'),
      cancelCallback,
      okCallback);
  showConfirmationOverlay();
}

/**
 * Click handler for the 'Remove selected items' button.
 * Confirms the deletion with the user, and then deletes the selected visits.
 */
function removeItems() {
  recordUmaAction('HistoryPage_RemoveSelected');
  if (!loadTimeData.getBoolean('allowDeletingHistory'))
    return;

  var checked = $('results-display').querySelectorAll(
      '.entry-box input[type=checkbox]:checked:not([disabled])');
  var disabledItems = [];
  var toBeRemoved = [];

  for (var i = 0; i < checked.length; i++) {
    var checkbox = checked[i];
    var entry = findAncestorByClass(checkbox, 'entry');
    toBeRemoved.push(entry.visit);

    // Disable the checkbox and put a strikethrough style on the link, so the
    // user can see what will be deleted.
    var link = entry.querySelector('a');
    checkbox.disabled = true;
    link.classList.add('to-be-removed');
    disabledItems.push(checkbox);
    var integerId = parseInt(entry.visit.id_, 10);
    // Record the ID of the entry to signify how many entries are above this
    // link on the page.
    recordUmaHistogram('HistoryPage.RemoveEntryPosition',
                       UMA_MAX_BUCKET_VALUE,
                       integerId);
    if (integerId <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      recordUmaHistogram('HistoryPage.RemoveEntryPositionSubset',
                         UMA_MAX_SUBSET_BUCKET_VALUE,
                         integerId);
    }
    if (entry.parentNode.className == 'search-results')
      recordUmaAction('HistoryPage_SearchResultRemove');
  }

  function onConfirmRemove() {
    recordUmaAction('HistoryPage_ConfirmRemoveSelected');
    historyModel.removeVisitsFromHistory(toBeRemoved,
        historyView.reload.bind(historyView));
    $('overlay').removeEventListener('cancelOverlay', onCancelRemove);
    hideConfirmationOverlay();
  }

  function onCancelRemove() {
    recordUmaAction('HistoryPage_CancelRemoveSelected');
    // Return everything to its previous state.
    for (var i = 0; i < disabledItems.length; i++) {
      var checkbox = disabledItems[i];
      checkbox.disabled = false;

      var entryBox = findAncestorByClass(checkbox, 'entry-box');
      entryBox.querySelector('a').classList.remove('to-be-removed');
    }
    $('overlay').removeEventListener('cancelOverlay', onCancelRemove);
    hideConfirmationOverlay();
  }

  if (checked.length) {
    confirmDeletion(onConfirmRemove, onCancelRemove);
    $('overlay').addEventListener('cancelOverlay', onCancelRemove);
  }
}

/**
 * Handler for the 'click' event on a checkbox.
 * @param {Event} e The click event.
 */
function checkboxClicked(e) {
  handleCheckboxStateChange(e.currentTarget, e.shiftKey);
}

/**
 * Post-process of checkbox state change. This handles range selection and
 * updates internal state.
 * @param {!HTMLInputElement} checkbox Clicked checkbox.
 * @param {boolean} shiftKey true if shift key is pressed.
 */
function handleCheckboxStateChange(checkbox, shiftKey) {
  updateParentCheckbox(checkbox);
  var id = Number(checkbox.id.slice('checkbox-'.length));
  // Handle multi-select if shift was pressed.
  if (shiftKey && (selectionAnchor != -1)) {
    var checked = checkbox.checked;
    // Set all checkboxes from the anchor up to the clicked checkbox to the
    // state of the clicked one.
    var begin = Math.min(id, selectionAnchor);
    var end = Math.max(id, selectionAnchor);
    for (var i = begin; i <= end; i++) {
      var checkbox = document.querySelector('#checkbox-' + i);
      if (checkbox) {
        checkbox.checked = checked;
        updateParentCheckbox(checkbox);
      }
    }
  }
  selectionAnchor = id;

  historyView.updateSelectionEditButtons();
}

/**
 * Handler for the 'click' event on a domain checkbox. Checkes or unchecks the
 * checkboxes of the visits to this domain in the respective group.
 * @param {Event} e The click event.
 */
function domainCheckboxClicked(e) {
  var siteEntry = findAncestorByClass(e.currentTarget, 'site-entry');
  var checkboxes =
      siteEntry.querySelectorAll('.site-results input[type=checkbox]');
  for (var i = 0; i < checkboxes.length; i++)
    checkboxes[i].checked = e.currentTarget.checked;
  historyView.updateSelectionEditButtons();
  // Stop propagation as clicking the checkbox would otherwise trigger the
  // group to collapse/expand.
  e.stopPropagation();
}

/**
 * Updates the domain checkbox for this visit checkbox if it has been
 * unchecked.
 * @param {Element} checkbox The checkbox that has been clicked.
 */
function updateParentCheckbox(checkbox) {
  if (checkbox.checked)
    return;

  var entry = findAncestorByClass(checkbox, 'site-entry');
  if (!entry)
    return;

  var groupCheckbox = entry.querySelector('.site-domain-wrapper input');
  if (groupCheckbox)
      groupCheckbox.checked = false;
}

function entryBoxMousedown(event) {
  // Prevent text selection when shift-clicking to select multiple entries.
  if (event.shiftKey)
    event.preventDefault();
}

/**
 * Handle click event for entryBox labels.
 * @param {!MouseEvent} event A click event.
 */
function entryBoxClick(event) {
  // Do nothing if a bookmark star is clicked.
  if (event.defaultPrevented)
    return;
  var element = event.target;
  // Do nothing if the event happened in an interactive element.
  for (; element != event.currentTarget; element = element.parentNode) {
    switch (element.tagName) {
      case 'A':
      case 'BUTTON':
      case 'INPUT':
        return;
    }
  }
  var checkbox = event.currentTarget.control;
  checkbox.checked = !checkbox.checked;
  handleCheckboxStateChange(checkbox, event.shiftKey);
  // We don't want to focus on the checkbox.
  event.preventDefault();
}

/**
 * Called when an individual history entry has been removed from the page.
 * This will only be called when all the elements affected by the deletion
 * have been removed from the DOM and the animations have completed.
 */
function onEntryRemoved() {
  historyView.updateSelectionEditButtons();
}

/**
 * Triggers a fade-out animation, and then removes |node| from the DOM.
 * @param {Node} node The node to be removed.
 * @param {Function?} onRemove A function to be called after the node
 *     has been removed from the DOM.
 */
function removeNode(node, onRemove) {
  node.classList.add('fade-out'); // Trigger CSS fade out animation.

  // Delete the node when the animation is complete.
  node.addEventListener('webkitTransitionEnd', function(e) {
    node.parentNode.removeChild(node);

    // In case there is nested deletion happening, prevent this event from
    // being handled by listeners on ancestor nodes.
    e.stopPropagation();

    if (onRemove)
      onRemove();
  });
}

/**
 * Removes a single entry from the view. Also removes gaps before and after
 * entry if necessary.
 * @param {Node} entry The DOM node representing the entry to be removed.
 */
function removeEntryFromView(entry) {
  var nextEntry = entry.nextSibling;
  var previousEntry = entry.previousSibling;
  var dayResults = findAncestorByClass(entry, 'day-results');

  var toRemove = [entry];

  // if there is no previous entry, and the next entry is a gap, remove it
  if (!previousEntry && nextEntry && nextEntry.className == 'gap')
    toRemove.push(nextEntry);

  // if there is no next entry, and the previous entry is a gap, remove it
  if (!nextEntry && previousEntry && previousEntry.className == 'gap')
    toRemove.push(previousEntry);

  // if both the next and previous entries are gaps, remove one
  if (nextEntry && nextEntry.className == 'gap' &&
      previousEntry && previousEntry.className == 'gap') {
    toRemove.push(nextEntry);
  }

  // If removing the last entry on a day, remove the entire day.
  if (dayResults && dayResults.querySelectorAll('.entry').length == 1) {
    toRemove.push(dayResults.previousSibling);  // Remove the 'h3'.
    toRemove.push(dayResults);
  }

  // Callback to be called when each node has finished animating. It detects
  // when all the animations have completed, and then calls |onEntryRemoved|.
  function onRemove() {
    for (var i = 0; i < toRemove.length; ++i) {
      if (toRemove[i].parentNode)
        return;
    }
    onEntryRemoved();
  }

  // Kick off the removal process.
  for (var i = 0; i < toRemove.length; ++i) {
    removeNode(toRemove[i], onRemove);
  }
}

/**
 * Toggles an element in the grouped history.
 * @param {Element} e The element which was clicked on.
 */
function toggleHandler(e) {
  var innerResultList = e.currentTarget.parentElement.querySelector(
      '.site-results');
  var innerArrow = e.currentTarget.parentElement.querySelector(
      '.site-domain-arrow');
  if (innerArrow.classList.contains('collapse')) {
    innerResultList.style.height = 'auto';
    // -webkit-transition does not work on height:auto elements so first set
    // the height to auto so that it is computed and then set it to the
    // computed value in pixels so the transition works properly.
    var height = innerResultList.clientHeight;
    innerResultList.style.height = 0;
    setTimeout(function() {
      innerResultList.style.height = height + 'px';
    }, 0);
    innerArrow.classList.remove('collapse');
    innerArrow.classList.add('expand');
  } else {
    innerResultList.style.height = 0;
    innerArrow.classList.remove('expand');
    innerArrow.classList.add('collapse');
  }
}

/**
 * Builds the DOM elements to show the managed status of a domain/URL.
 * @param {ManagedModeFilteringBehavior} filteringBehavior The filter behavior
 *     for this item.
 * @return {Element} Returns the DOM elements which show the status.
 */
function getManagedStatusDOM(filteringBehavior) {
  var filterStatusDiv = createElementWithClassName('div', 'filter-status');
  var filteringBehaviorDiv =
      createElementWithClassName('div', 'filtering-behavior');
  filterStatusDiv.appendChild(filteringBehaviorDiv);

  updateHostStatus(filterStatusDiv, filteringBehavior);
  return filterStatusDiv;
}


///////////////////////////////////////////////////////////////////////////////
// Chrome callbacks:

/**
 * Our history system calls this function with results from searches.
 * @param {Object} info An object containing information about the query.
 * @param {Array} results A list of results.
 */
function historyResult(info, results) {
  historyModel.addResults(info, results);
}

/**
 * Called by the history backend when history removal is successful.
 */
function deleteComplete() {
  historyModel.deleteComplete();
}

/**
 * Called by the history backend when history removal is unsuccessful.
 */
function deleteFailed() {
  window.console.log('Delete failed');
}

/**
 * Called when the history is deleted by someone else.
 */
function historyDeleted() {
  var anyChecked = document.querySelector('.entry input:checked') != null;
  // Reload the page, unless the user has any items checked.
  // TODO(dubroy): We should just reload the page & restore the checked items.
  if (!anyChecked)
    historyView.reload();
}

// Add handlers to HTML elements.
document.addEventListener('DOMContentLoaded', load);

// This event lets us enable and disable menu items before the menu is shown.
document.addEventListener('canExecute', function(e) {
  e.canExecute = true;
});
