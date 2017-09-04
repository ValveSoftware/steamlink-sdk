// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Element} root
 * @param {?Element} boundary
 * @param {cr.ui.FocusRow.Delegate} delegate
 * @constructor
 * @extends {cr.ui.FocusRow}
 */
function HistoryFocusRow(root, boundary, delegate) {
  cr.ui.FocusRow.call(this, root, boundary, delegate);
  this.addItems();
}

HistoryFocusRow.prototype = {
  __proto__: cr.ui.FocusRow.prototype,

  /** @override */
  getCustomEquivalent: function(sampleElement) {
    var equivalent;

    if (this.getTypeForElement(sampleElement) == 'star')
      equivalent = this.getFirstFocusable('title');

    return equivalent ||
        cr.ui.FocusRow.prototype.getCustomEquivalent.call(
            this, sampleElement);
  },

  addItems: function() {
    this.destroy();

    assert(this.addItem('checkbox', '#checkbox'));
    assert(this.addItem('title', '#title'));
    assert(this.addItem('menu-button', '#menu-button'));

    this.addItem('star', '#bookmark-star');
  },
};

cr.define('md_history', function() {
  /**
   * @param {{lastFocused: Object}} historyItemElement
   * @constructor
   * @implements {cr.ui.FocusRow.Delegate}
   */
  function FocusRowDelegate(historyItemElement) {
    this.historyItemElement = historyItemElement;
  }

  FocusRowDelegate.prototype = {
    /**
     * @override
     * @param {!cr.ui.FocusRow} row
     * @param {!Event} e
     */
    onFocus: function(row, e) {
      this.historyItemElement.lastFocused = e.path[0];
    },

    /**
     * @override
     * @param {!cr.ui.FocusRow} row The row that detected a keydown.
     * @param {!Event} e
     * @return {boolean} Whether the event was handled.
     */
    onKeydown: function(row, e) {
      // Prevent iron-list from changing the focus on enter.
      if (e.key == 'Enter')
        e.stopPropagation();

      return false;
    },
  };

  var HistoryItem = Polymer({
    is: 'history-item',

    properties: {
      // Underlying HistoryEntry data for this item. Contains read-only fields
      // from the history backend, as well as fields computed by history-list.
      item: {type: Object, observer: 'showIcon_'},

      // Search term used to obtain this history-item.
      searchTerm: {type: String},

      selected: {type: Boolean, reflectToAttribute: true},

      isCardStart: {type: Boolean, reflectToAttribute: true},

      isCardEnd: {type: Boolean, reflectToAttribute: true},

      // True if the item is being displayed embedded in another element and
      // should not manage its own borders or size.
      embedded: {type: Boolean, reflectToAttribute: true},

      hasTimeGap: {type: Boolean},

      numberOfItems: {type: Number},

      // The path of this history item inside its parent.
      path: String,

      index: Number,

      /** @type {Element} */
      lastFocused: {
        type: Object,
        notify: true,
      },

      ironListTabIndex: {
        type: Number,
        observer: 'ironListTabIndexChanged_',
      },
    },

    /** @private {?HistoryFocusRow} */
    row_: null,

    /** @override */
    attached: function() {
      Polymer.RenderStatus.afterNextRender(this, function() {
        this.row_ = new HistoryFocusRow(
            this.$['main-container'], null, new FocusRowDelegate(this));
        this.row_.makeActive(this.ironListTabIndex == 0);
        this.listen(this, 'focus', 'onFocus_');
        this.listen(this, 'dom-change', 'onDomChange_');
      });
    },

    /** @override */
    detached: function() {
      this.unlisten(this, 'focus', 'onFocus_');
      this.unlisten(this, 'dom-change', 'onDomChange_');
      if (this.row_)
        this.row_.destroy();
    },

    /**
     * @private
     */
    onFocus_: function() {
      if (this.lastFocused)
        this.row_.getEquivalentElement(this.lastFocused).focus();
      else
        this.row_.getFirstFocusable().focus();

      this.tabIndex = -1;
    },

    /**
     * @private
     */
    ironListTabIndexChanged_: function() {
      if (this.row_)
        this.row_.makeActive(this.ironListTabIndex == 0);
    },

    /**
     * @private
     */
    onDomChange_: function() {
      if (this.row_)
        this.row_.addItems();
    },

    /**
     * Toggle item selection whenever the checkbox or any non-interactive part
     * of the item is clicked.
     * @param {MouseEvent} e
     * @private
     */
    onItemClick_: function(e) {
      for (var i = 0; i < e.path.length; i++) {
        var elem = e.path[i];
        if (elem.id != 'checkbox' &&
            (elem.nodeName == 'A' || elem.nodeName == 'BUTTON')) {
          return;
        }
      }

      if (this.selectionNotAllowed_())
        return;

      this.$.checkbox.focus();
      this.fire('history-checkbox-select', {
        element: this,
        shiftKey: e.shiftKey,
      });
    },

    /**
     * @param {MouseEvent} e
     * @private
     */
    onItemMousedown_: function(e) {
      // Prevent shift clicking a checkbox from selecting text.
      if (e.shiftKey)
        e.preventDefault();
    },

    /**
     * @private
     * @return {string}
     */
    getEntrySummary_: function() {
      var item = this.item;
      return loadTimeData.getStringF(
          'entrySummary', item.dateTimeOfDay,
          item.starred ? loadTimeData.getString('bookmarked') : '', item.title,
          item.domain);
    },

    /**
     * @param {boolean} selected
     * @return {string}
     * @private
     */
    getAriaChecked_: function(selected) {
      return selected ? 'true' : 'false';
    },

    /**
     * Remove bookmark of current item when bookmark-star is clicked.
     * @private
     */
    onRemoveBookmarkTap_: function() {
      if (!this.item.starred)
        return;

      if (this.$$('#bookmark-star') == this.root.activeElement)
        this.$['menu-button'].focus();

      var browserService = md_history.BrowserService.getInstance();
      browserService.removeBookmark(this.item.url);
      browserService.recordAction('BookmarkStarClicked');

      this.fire('remove-bookmark-stars', this.item.url);
    },

    /**
     * Fires a custom event when the menu button is clicked. Sends the details
     * of the history item and where the menu should appear.
     */
    onMenuButtonTap_: function(e) {
      this.fire('toggle-menu', {
        target: Polymer.dom(e).localTarget,
        index: this.index,
        item: this.item,
        path: this.path,
      });

      // Stops the 'tap' event from closing the menu when it opens.
      e.stopPropagation();
    },

    /**
     * Record metrics when a result is clicked. This is deliberately tied to
     * on-click rather than on-tap, as on-click triggers from middle clicks.
     */
    onLinkClick_: function() {
      var browserService = md_history.BrowserService.getInstance();
      browserService.recordAction('EntryLinkClick');

      if (this.searchTerm)
        browserService.recordAction('SearchResultClick');

      if (this.index == undefined)
        return;

      browserService.recordHistogram(
          'HistoryPage.ClickPosition',
          Math.min(this.index, UMA_MAX_BUCKET_VALUE), UMA_MAX_BUCKET_VALUE);

      if (this.index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram(
            'HistoryPage.ClickPositionSubset', this.index,
            UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    },

    onLinkRightClick_: function() {
      md_history.BrowserService.getInstance().recordAction(
          'EntryLinkRightClick');
    },

    /**
     * Set the favicon image, based on the URL of the history item.
     * @private
     */
    showIcon_: function() {
      this.$.icon.style.backgroundImage = cr.icon.getFavicon(this.item.url);
    },

    selectionNotAllowed_: function() {
      return !loadTimeData.getBoolean('allowDeletingHistory');
    },

    /**
     * @param {number} numberOfItems The number of items in the card.
     * @param {string} historyDate Date of the current result.
     * @param {string} search The search term associated with these results.
     * @return {string} The title for this history card.
     * @private
     */
    cardTitle_: function(numberOfItems, historyDate, search) {
      if (!search)
        return this.item.dateRelativeDay;
      return HistoryItem.searchResultsTitle(numberOfItems, search);
    },

    /** @private */
    addTimeTitle_: function() {
      var el = this.$['time-accessed'];
      el.setAttribute('title', new Date(this.item.time).toString());
      this.unlisten(el, 'mouseover', 'addTimeTitle_');
    },
  });

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {Array<HistoryEntry>} visits
   * @param {number} currentIndex
   * @param {string} searchedTerm
   * @return {boolean} Whether or not time gap separator is required.
   */
  HistoryItem.needsTimeGap = function(visits, currentIndex, searchedTerm) {
    if (currentIndex >= visits.length - 1 || visits.length == 0)
      return false;

    var currentItem = visits[currentIndex];
    var nextItem = visits[currentIndex + 1];

    if (searchedTerm)
      return currentItem.dateShort != nextItem.dateShort;

    return currentItem.time - nextItem.time > BROWSING_GAP_TIME &&
        currentItem.dateRelativeDay == nextItem.dateRelativeDay;
  };

  /**
   * @param {number} numberOfResults
   * @param {string} searchTerm
   * @return {string} The title for a page of search results.
   */
  HistoryItem.searchResultsTitle = function(numberOfResults, searchTerm) {
    var resultId = numberOfResults == 1 ? 'searchResult' : 'searchResults';
    return loadTimeData.getStringF('foundSearchResults', numberOfResults,
        loadTimeData.getString(resultId), searchTerm);
  };

  return { HistoryItem: HistoryItem };
});
