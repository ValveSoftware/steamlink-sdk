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
   * Creates a new Most Visited object for tiling.
   * @constructor
   * @extends {HTMLAnchorElement}
   */
  function MostVisited() {
    var el = cr.doc.createElement('a');
    el.__proto__ = MostVisited.prototype;
    el.initialize();

    return el;
  }

  MostVisited.prototype = {
    __proto__: HTMLAnchorElement.prototype,

    initialize: function() {
      this.reset();

      this.addEventListener('click', this.handleClick_);
      this.addEventListener('keydown', this.handleKeyDown_);
      this.addEventListener('mouseover', this.handleMouseOver_);
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
     * for a blank thumbnail.
     */
    reset: function() {
      this.className = 'most-visited filler real';
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
          '<span class="title"></span>';

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
      this.id = 'most-visited-tile-' + id;
      this.data_ = data;
      this.classList.add('focusable');

      var faviconDiv = this.querySelector('.favicon');
      faviconDiv.style.backgroundImage = getFaviconImageSet(data.url);

      // The favicon should have the same dominant color regardless of the
      // device pixel ratio the favicon is requested for.
      chrome.send('getFaviconDominantColor',
                  [getFaviconUrlForCurrentDevicePixelRatio(data.url), this.id]);

      var title = this.querySelector('.title');
      title.textContent = data.title;
      title.dir = data.direction;

      // Sets the tooltip.
      this.title = data.title;

      var thumbnailUrl = 'chrome://thumb/' + data.url;
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
        ntp.logTimeToClick('MostVisited');
        // Records an app launch from the most visited page (Chrome will decide
        // whether the url is an app). TODO(estade): this only works for clicks;
        // other actions like "open in new tab" from the context menu won't be
        // recorded. Can this be fixed?
        chrome.send('recordAppLaunchByURL',
                    [encodeURIComponent(this.href),
                     ntp.APP_LAUNCH.NTP_MOST_VISITED]);
        // Records the index of this tile.
        chrome.send('metricsHandler:recordInHistogram',
                    ['NewTabPage.MostVisited', this.index, 8]);
        // Records the action. This will be available as a time-stamped stream
        // server-side and can be used to compute time-to-long-dwell.
        chrome.send('metricsHandler:recordAction', ['MostVisited_Clicked']);
        chrome.send('mostVisitedAction',
                    [ntp.NtpFollowAction.CLICKED_TILE]);
      }
    },

    /**
     * Allow blacklisting most visited site using the keyboard.
     */
    handleKeyDown_: function(e) {
      if (!cr.isMac && e.keyCode == 46 || // Del
          cr.isMac && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
        this.blacklist_();
      }
    },

    /**
     * The mouse has entered a Most Visited tile div. Only log the first
     * mouseover event. By doing this we solve the issue with the mouseover
     * event listener that bubbles up to the parent, which would cause it to
     * fire multiple times even if the mouse stays within one tile.
     */
    handleMouseOver_: function(e) {
      var self = this;
      var ancestor = findAncestor(e.relatedTarget, function(node) {
        return node == self;
      });
      // If ancestor is null, mouse is entering the parent element.
      if (ancestor == null)
        chrome.send('metricsHandler:logMouseover');
    },

    /**
     * Permanently removes a page from Most Visited.
     */
    blacklist_: function() {
      this.showUndoNotification_();
      chrome.send('blacklistURLFromMostVisited', [this.data_.url]);
      this.reset();
      chrome.send('getMostVisited');
      this.classList.add('blacklisted');
    },

    showUndoNotification_: function() {
      var data = this.data_;
      var self = this;
      var doUndo = function() {
        chrome.send('removeURLsFromMostVisitedBlacklist', [data.url]);
        self.updateForData(data);
      }

      var undo = {
        action: doUndo,
        text: loadTimeData.getString('undothumbnailremove'),
      };

      var undoAll = {
        action: function() {
          chrome.send('clearMostVisitedURLsBlacklist');
        },
        text: loadTimeData.getString('restoreThumbnailsShort'),
      };

      ntp.showNotification(
          loadTimeData.getString('thumbnailremovednotification'),
          [undo, undoAll]);
    },

    /**
     * Set the size and position of the most visited tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = toCssPx(size);
      this.style.height = toCssPx(heightForWidth(size));

      this.style.left = toCssPx(x);
      this.style.right = toCssPx(x);
      this.style.top = toCssPx(y);
    },

    /**
     * Returns whether this element can be 'removed' from chrome (i.e. whether
     * the user can drag it onto the trash and expect something to happen).
     * @return {boolean} True, since most visited pages can always be
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
     */
    setDragData: function(dataTransfer) {
      dataTransfer.setData('Text', this.data_.title);
      dataTransfer.setData('URL', this.data_.url);
    },
  };

  var mostVisitedPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 2,
    // The most tiles we will show in a row.
    maxColCount: 4,

    // The smallest a tile can be.
    minTileWidth: 122,
    // The biggest a tile can be. 212 (max thumbnail width) + 2.
    maxTileWidth: 214,

    // The padding between tiles, as a fraction of the tile width.
    tileSpacingFraction: 1 / 8,
  };
  TilePage.initGridValues(mostVisitedPageGridValues);

  /**
   * Calculates the height for a Most Visited tile for a given width. The size
   * is based on the thumbnail, which should have a 212:132 ratio.
   * @return {number} The height.
   */
  function heightForWidth(width) {
    // The 2s are for borders, the 31 is for the title.
    return (width - 2) * 132 / 212 + 2 + 31;
  }

  var THUMBNAIL_COUNT = 8;

  /**
   * Creates a new MostVisitedPage object.
   * @constructor
   * @extends {TilePage}
   */
  function MostVisitedPage() {
    var el = new TilePage(mostVisitedPageGridValues);
    el.__proto__ = MostVisitedPage.prototype;
    el.initialize();

    return el;
  }

  MostVisitedPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('most-visited-page');
      this.data_ = null;
      this.mostVisitedTiles_ = this.getElementsByClassName('most-visited real');

      this.addEventListener('carddeselected', this.handleCardDeselected_);
      this.addEventListener('cardselected', this.handleCardSelected_);
    },

    /**
     * Create blank (filler) tiles.
     * @private
     */
    createTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        this.appendTile(new MostVisited());
      }
    },

    /**
     * Update the tiles after a change to |data_|.
     */
    updateTiles_: function() {
      for (var i = 0; i < THUMBNAIL_COUNT; i++) {
        var page = this.data_[i];
        var tile = this.mostVisitedTiles_[i];

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
        chrome.send('mostVisitedAction',
                    [ntp.NtpFollowAction.CLICKED_OTHER_NTP_PANE]);
      }
    },

    /**
     * Handles the 'card selected' event (i.e. the user clicked to select the
     * Most Visited pane).
     * @param {Event} e The CardChanged event.
     */
    handleCardSelected_: function(e) {
      if (!document.documentElement.classList.contains('starting-up'))
        chrome.send('mostVisitedSelected');
    },

    /**
     * Array of most visited data objects.
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
      logEvent('mostVisited.layout: ' + (Date.now() - startTime));
    },

    /** @override */
    shouldAcceptDrag: function(e) {
      return false;
    },

    /** @override */
    heightForWidth: heightForWidth,
  };

  /**
   * Executed once the NTP has loaded. Checks if the Most Visited pane is
   * shown or not. If it is shown, the 'mostVisitedSelected' message is sent
   * to the C++ code, to record the fact that the user has seen this pane.
   */
  MostVisitedPage.onLoaded = function() {
    if (ntp.getCardSlider() &&
        ntp.getCardSlider().currentCardValue &&
        ntp.getCardSlider().currentCardValue.classList
        .contains('most-visited-page')) {
      chrome.send('mostVisitedSelected');
    }
  }

  /**
   * We've gotten additional Most Visited data. Update our old data with the
   * new data. The ordering of the new data is not important, except when a
   * page is pinned. Thus we try to minimize re-ordering.
   * @param {Array} oldData The current Most Visited page list.
   * @param {Array} newData The new Most Visited page list.
   * @return {Array} The merged page list that should replace the current page
   *     list.
   */
  function refreshData(oldData, newData) {
    oldData = oldData.slice(0, THUMBNAIL_COUNT);
    newData = newData.slice(0, THUMBNAIL_COUNT);

    // Copy over pinned sites directly.
    for (var j = 0; j < newData.length; j++) {
      if (newData[j].pinned) {
        oldData[j] = newData[j];
        // Mark the entry as 'updated' so we don't try to update again.
        oldData[j].updated = true;
        // Mark the newData page as 'used' so we don't try to re-use it.
        newData[j].used = true;
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
  };

  return {
    MostVisitedPage: MostVisitedPage,
    refreshData: refreshData,
  };
});

document.addEventListener('ntpLoaded', ntp.MostVisitedPage.onLoaded);
