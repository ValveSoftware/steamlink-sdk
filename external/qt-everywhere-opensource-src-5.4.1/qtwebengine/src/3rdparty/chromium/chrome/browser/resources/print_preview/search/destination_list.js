// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that displays a list of destinations with a heading, action link,
   * and "Show All..." button. An event is dispatched when the action link is
   * activated.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @param {string} title Title of the destination list.
   * @param {string=} opt_actionLinkLabel Optional label of the action link. If
   *     no label is provided, the action link will not be shown.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationList(eventTarget, title, opt_actionLinkLabel) {
    print_preview.Component.call(this);

    /**
     * Event target to pass to destination items for dispatching SELECT events.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Title of the destination list.
     * @type {string}
     * @private
     */
    this.title_ = title;

    /**
     * Label of the action link.
     * @type {?string}
     * @private
     */
    this.actionLinkLabel_ = opt_actionLinkLabel || null;

    /**
     * Backing store for the destination list.
     * @type {!Array.<print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Current query used for filtering.
     * @type {RegExp}
     * @private
     */
    this.query_ = null;

    /**
     * Whether the destination list is fully expanded.
     * @type {boolean}
     * @private
     */
    this.isShowAll_ = false;

    /**
     * Maximum number of destinations before showing the "Show All..." button.
     * @type {number}
     * @private
     */
    this.shortListSize_ = DestinationList.DEFAULT_SHORT_LIST_SIZE_;
  };

  /**
   * Enumeration of event types dispatched by the destination list.
   * @enum {string}
   */
  DestinationList.EventType = {
    // Dispatched when the action linked is activated.
    ACTION_LINK_ACTIVATED: 'print_preview.DestinationList.ACTION_LINK_ACTIVATED'
  };

  /**
   * Default maximum number of destinations before showing the "Show All..."
   * button.
   * @type {number}
   * @const
   * @private
   */
  DestinationList.DEFAULT_SHORT_LIST_SIZE_ = 4;

  /**
   * Height of a destination list item in pixels.
   * @type {number}
   * @const
   * @private
   */
  DestinationList.HEIGHT_OF_ITEM_ = 30;

  DestinationList.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isShowAll Whether the show-all button is activated. */
    setIsShowAll: function(isShowAll) {
      this.isShowAll_ = isShowAll;
      this.renderDestinations_();
    },

    /**
     * @return {number} Size of list when destination list is in collapsed
     *     mode (a.k.a non-show-all mode).
     */
    getShortListSize: function() {
      return this.shortListSize_;
    },

    /** @return {number} Count of the destinations in the list. */
    getDestinationsCount: function() {
      return this.destinations_.length;
    },

    /**
     * Gets estimated height of the destination list for the given number of
     * items.
     * @param {number} Number of items to render in the destination list.
     * @return {number} Height (in pixels) of the destination list.
     */
    getEstimatedHeightInPixels: function(numItems) {
      numItems = Math.min(numItems, this.destinations_.length);
      var headerHeight =
          this.getChildElement('.destination-list > header').offsetHeight;
      return headerHeight + numItems * DestinationList.HEIGHT_OF_ITEM_;
    },

    /** @param {boolean} isVisible Whether the throbber is visible. */
    setIsThrobberVisible: function(isVisible) {
      setIsVisible(this.getChildElement('.throbber-container'), isVisible);
    },

    /**
     * @param {number} size Size of list when destination list is in collapsed
     *     mode (a.k.a non-show-all mode).
     */
    updateShortListSize: function(size) {
      size = Math.max(1, Math.min(size, this.destinations_.length));
      if (size == 1 && this.destinations_.length > 1) {
        // If this is the case, we will only show the "Show All" button and
        // nothing else. Increment the short list size by one so that we can see
        // at least one print destination.
        size++;
      }
      this.setShortListSizeInternal(size);
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-template'));
      this.getChildElement('.title').textContent = this.title_;
      if (this.actionLinkLabel_) {
        var actionLinkEl = this.getChildElement('.action-link');
        actionLinkEl.textContent = this.actionLinkLabel_;
        setIsVisible(actionLinkEl, true);
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.getChildElement('.action-link'),
          'click',
          this.onActionLinkClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.show-all-button'),
          'click',
          this.setIsShowAll.bind(this, true));
    },

    /**
     * Updates the destinations to render in the destination list.
     * @param {!Array.<print_preview.Destination>} destinations Destinations to
     *     render.
     */
    updateDestinations: function(destinations) {
      this.destinations_ = destinations;
      this.renderDestinations_();
    },

    /** @param {?string} query Query to update the filter with. */
    updateSearchQuery: function(query) {
      if (!query) {
        this.query_ = null;
      } else {
        // Generate regexp-safe query by escaping metacharacters.
        var safeQuery = query.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
        this.query_ = new RegExp('(' + safeQuery + ')', 'ig');
      }
      this.renderDestinations_();
    },

    /**
     * @param {string} text Text to set the action link to.
     * @protected
     */
    setActionLinkTextInternal: function(text) {
      this.actionLinkLabel_ = text;
      this.getChildElement('.action-link').textContent = text;
    },

    /**
     * Sets the short list size without constraints.
     * @protected
     */
    setShortListSizeInternal: function(size) {
      this.shortListSize_ = size;
      this.renderDestinations_();
    },

    /**
     * Renders all destinations in the given list.
     * @param {!Array.<print_preview.Destination>} destinations List of
     *     destinations to render.
     * @protected
     */
    renderListInternal: function(destinations) {
      setIsVisible(this.getChildElement('.no-destinations-message'),
                   destinations.length == 0);
      setIsVisible(this.getChildElement('.destination-list > footer'), false);
      var numItems = destinations.length;
      if (destinations.length > this.shortListSize_ && !this.isShowAll_) {
        numItems = this.shortListSize_ - 1;
        this.getChildElement('.total').textContent =
            localStrings.getStringF('destinationCount', destinations.length);
        setIsVisible(this.getChildElement('.destination-list > footer'), true);
      }
      for (var i = 0; i < numItems; i++) {
        var destListItem = new print_preview.DestinationListItem(
            this.eventTarget_, destinations[i], this.query_);
        this.addChild(destListItem);
        destListItem.render(this.getChildElement('.destination-list > ul'));
      }
    },

    /**
     * Renders all destinations in the list that match the current query. For
     * each render, all old destination items are first removed.
     * @private
     */
    renderDestinations_: function() {
      this.removeChildren();

      if (!this.query_) {
        this.renderListInternal(this.destinations_);
      } else {
        var filteredDests = this.destinations_.filter(function(destination) {
          return destination.matches(this.query_);
        }, this);
        this.renderListInternal(filteredDests);
      }
    },

    /**
     * Called when the action link is clicked. Dispatches an
     * ACTION_LINK_ACTIVATED event.
     * @private
     */
    onActionLinkClick_: function() {
      cr.dispatchSimpleEvent(this,
                             DestinationList.EventType.ACTION_LINK_ACTIVATED);
    }
  };

  // Export
  return {
    DestinationList: DestinationList
  };
});
