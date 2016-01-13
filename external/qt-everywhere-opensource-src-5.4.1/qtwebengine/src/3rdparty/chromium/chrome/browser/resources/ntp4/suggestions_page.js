// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp', function() {
  'use strict';

  var TilePage = ntp.TilePage;

  /**
   * A counter for generating unique tile IDs.
   */
  var tileID = 0;

  /**
   * Creates a new Suggestions page object for tiling.
   * @constructor
   * @extends {HTMLAnchorElement}
   */
  function Suggestion() {
    var el = cr.doc.createElement('a');
    el.__proto__ = Suggestion.prototype;
    el.initialize();

    return el;
  }

  Suggestion.prototype = {
    __proto__: HTMLAnchorElement.prototype,

    initialize: function() {
      this.reset();

      this.addEventListener('click', this.handleClick_);
      this.addEventListener('keydown', this.handleKeyDown_);
    },

    get index() {
      assert(this.tile);
      return this.tile.index;
    },

    get data() {
      return this.data_;
    },

    /**
     * Clears the DOM hierarchy for this node, setting it back to the default
     * for a blank thumbnail. TODO(georgey) make it a template.
     */
    reset: function() {
      this.className = 'suggestions filler real';
      this.innerHTML =
          '<span class="thumbnail-wrapper fills-parent">' +
            '<div class="close-button"></div>' +
            '<span class="thumbnail fills-parent">' +
              // thumbnail-shield provides a gradient fade effect.
              '<div class="thumbnail-shield fills-parent"></div>' +
            '</span>' +
            '<span class="favicon"></span>' +
          '</span>' +
          '<div class="color-stripe"></div>' +
          '<span class="title"></span>' +
          '<span class="score"></span>';

      this.querySelector('.close-button').title =
          loadTimeData.getString('removethumbnailtooltip');

      this.tabIndex = -1;
      this.data_ = null;
      this.removeAttribute('id');
      this.title = '';
    },

    /**
     * Update the appearance of this tile according to |data|.
     * @param {Object} data A dictionary of relevant data for the page.
     */
    updateForData: function(data) {
      if (this.classList.contains('blacklisted') && data) {
        // Animate appearance of new tile.
        this.classList.add('new-tile-contents');
      }
      this.classList.remove('blacklisted');

      if (!data || data.filler) {
        if (this.data_)
          this.reset();
        return;
      }

      var id = tileID++;
      this.id = 'suggestions-tile-' + id;
      this.data_ = data;
      this.classList.add('focusable');

      var faviconDiv = this.querySelector('.favicon');
      var faviconUrl = 'chrome://favicon/size/16@1x/' + data.url;
      faviconDiv.style.backgroundImage = url(faviconUrl);
      chrome.send('getFaviconDominantColor', [faviconUrl, this.id]);

      var title = this.querySelector('.title');
      title.textContent = data.title;
      title.dir = data.direction;

      var score = this.querySelector('.score');
      score.textContent = data.score;

      // Sets the tooltip.
      this.title = data.title;

      var thumbnailUrl;
      thumbnailUrl = data.urlImage ? data.urlImage :
        'chrome://thumb/' + data.url;

      this.querySelector('.thumbnail').style.backgroundImage =
          url(thumbnailUrl);

      this.href = data.url;

      this.classList.remove('filler');
    },

    /**
     * Sets the color of the favicon dominant color bar.
     * @param {string} color The css-parsable value for the color.
     */
    set stripeColor(color) {
      this.querySelector('.color-stripe').style.backgroundColor = color;
    },

    /**
     * Handles a click on the tile.
     * @param {Event} e The click event.
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        this.blacklist_();
        e.preventDefault();
      } else {
        // Records the index of this tile.
        chrome.send('metricsHandler:recordInHistogram',
                    ['NewTabPage.SuggestedSite', this.index, 8]);
        chrome.send('suggestedSitesAction',
                    [ntp.NtpFollowAction.CLICKED_TILE]);
      }
    },

    /**
     * Allow blacklisting suggestions site using the keyboard.
     * @param {Event} e The keydown event.
     */
    handleKeyDown_: function(e) {
      if (!cr.isMac && e.keyCode == 46 || // Del
          cr.isMac && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
        this.blacklist_();
      }
    },

    /**
     * Permanently removes a page from Suggestions.
     */
    blacklist_: function() {
      this.showUndoNotification_();
      chrome.send('blacklistURLFromSuggestions', [this.data_.url]);
      this.reset();
      chrome.send('getSuggestions');
      this.classList.add('blacklisted');
    },

    /**
     * Shows notification that you can undo blacklisting.
     */
    showUndoNotification_: function() {
      var data = this.data_;
      var self = this;
      var doUndo = function() {
        chrome.send('removeURLsFromSuggestionsBlacklist', [data.url]);
        self.updateForData(data);
      };

      var undo = {
        action: doUndo,
        text: loadTimeData.getString('undothumbnailremove'),
      };

      var undoAll = {
        action: function() {
          chrome.send('clearSuggestionsURLsBlacklist');
        },
        text: loadTimeData.getString('restoreThumbnailsShort'),
      };

      ntp.showNotification(
          loadTimeData.getString('thumbnailremovednotification'),
          [undo, undoAll]);
    },

    /**
     * Set the size and position of the suggestions tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     */
    setBounds: function(size, x, y) {
      this.style.width = size + 'px';
      this.style.height = heightForWidth(size) + 'px';

      this.style.left = x + 'px';
      this.style.right = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Returns whether this element can be 'removed' from chrome (i.e. whether
     * the user can drag it onto the trash and expect something to happen).
     * @return {boolean} True, since suggestions pages can always be
     *     blacklisted.
     */
    canBeRemoved: function() {
      return true;
    },

    /**
     * Removes this element from chrome, i.e. blacklists it.
     */
    removeFromChrome: function() {
      this.blacklist_();
      this.parentNode.classList.add('finishing-drag');
    },

    /**
     * Called when a drag of this tile has ended (after all animations have
     * finished).
     */
    finalizeDrag: function() {
      this.parentNode.classList.remove('finishing-drag');
    },

    /**
     * Called when a drag is starting on the tile. Updates dataTransfer with
     * data for this tile (for dragging outside of the NTP).
     * @param {Event.DataTransfer} dataTransfer The drag event data store.
     */
    setDragData: function(dataTransfer) {
      dataTransfer.setData('Text', this.data_.title);
      dataTransfer.setData('URL', this.data_.url);
    },
  };

  var suggestionsPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 2,
    // The suggestions we will show in a row.
    maxColCount: 4,

    // The smallest a tile can be.
    minTileWidth: 122,
    // The biggest a tile can be. 212 (max thumbnail width) + 2.
    maxTileWidth: 214,

    // The padding between tiles, as a fraction of the tile width.
    tileSpacingFraction: 1 / 8,
  };
  TilePage.initGridValues(suggestionsPageGridValues);

  /**
   * Calculates the height for a Suggestion tile for a given width. The size
   * is based on the thumbnail, which should have a 212:132 ratio.
   * @return {number} The height.
   */
  function heightForWidth(width) {
    // The 2s are for borders, the 36 is for the title and score.
    return (width - 2) * 132 / 212 + 2 + 36;
  }

  var THUMBNAIL_COUNT = 8;

  /**
   * Creates a new SuggestionsPage object.
   * @constructor
   * @extends {TilePage}
   */
  function SuggestionsPage() {
    var el = new TilePage(suggestionsPageGridValues);
    el.__proto__ = SuggestionsPage.prototype;
    el.initialize();

    return el;
  }

  SuggestionsPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('suggestions-page');
      this.data_ = null;
      this.suggestionsTiles_ = this.getElementsByClassName('suggestions real');

      this.addEventListener('carddeselected', this.handleCardDeselected_);
      this.addEventListener('cardselected', this.handleCardSelected_);
    },

    /**
     * Create blank (filler) tiles.
     * @private
     */
    createTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        this.appendTile(new Suggestion());
      }
    },

    /**
     * Update the tiles after a change to |this.data_|.
     */
    updateTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        var page = this.data_[i];
        var tile = this.suggestionsTiles_[i];

        if (i >= this.data_.length)
          tile.reset();
        else
          tile.updateForData(page);
      }
    },

    /**
     * Handles the 'card deselected' event (i.e. the user clicked to another
     * pane).
     * @param {Event} e The CardChanged event.
     */
    handleCardDeselected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up')) {
        chrome.send('suggestedSitesAction',
                    [ntp.NtpFollowAction.CLICKED_OTHER_NTP_PANE]);
      }
    },

    /**
     * Handles the 'card selected' event (i.e. the user clicked to select the
     * Suggested pane).
     * @param {Event} e The CardChanged event.
     */
    handleCardSelected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up'))
        chrome.send('suggestedSitesSelected');
    },

    /**
     * Array of suggestions data objects.
     * @type {Array}
     */
    get data() {
      return this.data_;
    },
    set data(data) {
      var startTime = Date.now();

      // The first time data is set, create the tiles.
      if (!this.data_) {
        this.createTiles_();
        this.data_ = data.slice(0, THUMBNAIL_COUNT);
      } else {
        this.data_ = refreshData(this.data_, data);
      }

      this.updateTiles_();
      this.updateFocusableElement();
      logEvent('suggestions.layout: ' + (Date.now() - startTime));
    },

    /** @override */
    shouldAcceptDrag: function(e) {
      return false;
    },

    /** @override */
    heightForWidth: heightForWidth,
  };

  /**
   * Executed once the NTP has loaded. Checks if the Suggested pane is
   * shown or not. If it is shown, the 'suggestedSitesSelected' message is sent
   * to the C++ code, to record the fact that the user has seen this pane.
   */
  SuggestionsPage.onLoaded = function() {
    if (ntp.getCardSlider() &&
        ntp.getCardSlider().currentCardValue &&
        ntp.getCardSlider().currentCardValue.classList
        .contains('suggestions-page')) {
      chrome.send('suggestedSitesSelected');
    }
  }

  /**
   * We've gotten additional data for Suggestions page. Update our old data with
   * the new data. The ordering of the new data is not important, except when a
   * page is pinned. Thus we try to minimize re-ordering.
   * @param {Array} oldData The current Suggestions page list.
   * @param {Array} newData The new Suggestions page list.
   * @return {Array} The merged page list that should replace the current page
   * list.
   */
  function refreshData(oldData, newData) {
    oldData = oldData.slice(0, THUMBNAIL_COUNT);
    newData = newData.slice(0, THUMBNAIL_COUNT);

    // Copy over pinned sites directly.
    for (var i = 0; i < newData.length; i++) {
      if (newData[i].pinned) {
        oldData[i] = newData[i];
        // Mark the entry as 'updated' so we don't try to update again.
        oldData[i].updated = true;
        // Mark the newData page as 'used' so we don't try to re-use it.
        newData[i].used = true;
      }
    }

    // Look through old pages; if they exist in the newData list, keep them
    // where they are.
    for (var i = 0; i < oldData.length; i++) {
      if (!oldData[i] || oldData[i].updated)
        continue;

      for (var j = 0; j < newData.length; j++) {
        if (newData[j].used)
          continue;

        if (newData[j].url == oldData[i].url) {
          // The background image and other data may have changed.
          oldData[i] = newData[j];
          oldData[i].updated = true;
          newData[j].used = true;
          break;
        }
      }
    }

    // Look through old pages that haven't been updated yet; replace them.
    for (var i = 0; i < oldData.length; i++) {
      if (oldData[i] && oldData[i].updated)
        continue;

      for (var j = 0; j < newData.length; j++) {
        if (newData[j].used)
          continue;

        oldData[i] = newData[j];
        oldData[i].updated = true;
        newData[j].used = true;
        break;
      }

      if (oldData[i] && !oldData[i].updated)
        oldData[i] = null;
    }

    // Clear 'updated' flags so this function will work next time it's called.
    for (var i = 0; i < THUMBNAIL_COUNT; i++) {
      if (oldData[i])
        oldData[i].updated = false;
    }

    return oldData;
  }

  return {
    SuggestionsPage: SuggestionsPage,
    refreshData: refreshData,
  };
});

document.addEventListener('ntpLoaded', ntp.SuggestionsPage.onLoaded);
