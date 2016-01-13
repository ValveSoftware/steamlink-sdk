// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a search box for searching through destinations.
   * @constructor
   * @extends {print_preview.Component}
   */
  function SearchBox() {
    print_preview.Component.call(this);

    /**
     * Timeout used to control incremental search.
     * @type {?number}
     * @private
     */
     this.timeout_ = null;

    /**
     * Input box where the query is entered.
     * @type {HTMLInputElement}
     * @private
     */
    this.input_ = null;
  };

  /**
   * Enumeration of event types dispatched from the search box.
   * @enum {string}
   */
  SearchBox.EventType = {
    SEARCH: 'print_preview.SearchBox.SEARCH'
  };

  /**
   * CSS classes used by the search box.
   * @enum {string}
   * @private
   */
  SearchBox.Classes_ = {
    INPUT: 'search-box-input'
  };

  /**
   * Delay in milliseconds before dispatching a SEARCH event.
   * @type {number}
   * @const
   * @private
   */
  SearchBox.SEARCH_DELAY_ = 150;

  SearchBox.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {string} New query to set the search box's query to. */
    setQuery: function(query) {
      query = query || '';
      this.input_.value = query.trim();
    },

    /** Sets the input element of the search box in focus. */
    focus: function() {
      this.input_.focus();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.input_, 'input', this.onInputInput_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.input_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.input_ = this.getElement().getElementsByClassName(
          SearchBox.Classes_.INPUT)[0];
    },

    /**
     * @return {string} The current query of the search box.
     * @private
     */
    getQuery_: function() {
      return this.input_.value.trim();
    },

    /**
     * Dispatches a SEARCH event.
     * @private
     */
    dispatchSearchEvent_: function() {
      this.timeout_ = null;
      var searchEvent = new Event(SearchBox.EventType.SEARCH);
      searchEvent.query = this.getQuery_();
      this.dispatchEvent(searchEvent);
    },

    /**
     * Called when the input element's value changes. Dispatches a search event.
     * @private
     */
    onInputInput_: function() {
      if (this.timeout_) {
        clearTimeout(this.timeout_);
      }
      this.timeout_ = setTimeout(
          this.dispatchSearchEvent_.bind(this), SearchBox.SEARCH_DELAY_);
    }
  };

  // Export
  return {
    SearchBox: SearchBox
  };
});
