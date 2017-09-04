// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Section IDs use for metrics.  The integer values should match up with the
 * |SettingsSections| in histograms.xml.
 * @type {Object<string, number>}
 */
var SettingsSections = {
  'None': 0,
  'Unknown': 1,
  'network-section-cros': 2,
  'proxy-section': 3,
  'appearance-section': 4,
  'device-section': 5,
  'search-section': 6,
  'sync-users-section': 7,
  'set-default-browser-section': 8,
  'date-time-section': 9,
  'device-control-section': 10,
  'privacy-section': 11,
  'bluetooth-devices': 12,
  'passwords-and-autofill-section': 13,
  'easy-unlock-section': 14,
  'web-content-section': 15,
  'network-section': 16,
  'languages-section': 17,
  'downloads-section': 18,
  'certificates-section': 19,
  'cloudprint-options-mdns': 20,
  'a11y-section': 21,
  'factory-reset-section': 22,
  'system-section': 23,
  'reset-profile-settings-section': 24,
  'sync-section': 25,
  'startup-section': 26,
  'mouselock-section': 27,
  'page-zoom-levels': 28,
  'status-section': 29,
  'main-section': 30,
  'pointer-section-touchpad': 31,
  'pointer-section-mouse': 32,
  'prefs-blocked-languages': 33,
  'prefs-language-blacklist': 34,
  'prefs-site-blacklist': 35,
  'prefs-whitelists': 36,
  'prefs-supported-languages': 37,
  'prefs-cld-version': 38,
  'prefs-cld-data-source': 39,
  'prefs-dump': 40,
};

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * Encapsulated handling of a search bubble.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function SearchBubble(text) {
    var el = cr.doc.createElement('div');
    SearchBubble.decorate(el);
    el.content = text;
    return el;
  }

  /**
   * Prohibit search for guests on desktop.
   */
  function ShouldEnableSearch() {
    return !loadTimeData.getBoolean('profileIsGuest') || cr.isChromeOS;
  }

  SearchBubble.decorate = function(el) {
    el.__proto__ = SearchBubble.prototype;
    el.decorate();
  };

  SearchBubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'search-bubble';

      this.innards_ = cr.doc.createElement('div');
      this.innards_.className = 'search-bubble-innards';
      this.appendChild(this.innards_);

      // We create a timer to periodically update the position of the bubbles.
      // While this isn't all that desirable, it's the only sure-fire way of
      // making sure the bubbles stay in the correct location as sections
      // may dynamically change size at any time.
      this.intervalId = setInterval(this.updatePosition.bind(this), 250);

      this.addEventListener('mouseover', function() {
        this.innards_.classList.toggle('above');
        this.updatePosition();
      });
    },

    /**
     * Sets the text message in the bubble.
     * @param {string} text The text the bubble will show.
     */
    set content(text) {
      this.innards_.textContent = text;
    },

    /**
     * Attach the bubble to the element.
     */
    attachTo: function(element) {
      var parent = element.parentElement;
      if (!parent)
        return;
      if (parent.tagName == 'TD') {
        // To make absolute positioning work inside a table cell we need
        // to wrap the bubble div into another div with position:relative.
        // This only works properly if the element is the first child of the
        // table cell which is true for all options pages.
        this.wrapper = cr.doc.createElement('div');
        this.wrapper.className = 'search-bubble-wrapper';
        this.wrapper.appendChild(this);
        parent.insertBefore(this.wrapper, element);
      } else {
        parent.insertBefore(this, element);
      }
    },

    /**
     * Clear the interval timer and remove the element from the page.
     */
    dispose: function() {
      clearInterval(this.intervalId);

      var child = this.wrapper || this;
      var parent = child.parentNode;
      if (parent)
        parent.removeChild(child);
    },

    /**
     * Update the position of the bubble.  Called at creation time and then
     * periodically while the bubble remains visible.
     */
    updatePosition: function() {
      // This bubble is 'owned' by the next sibling.
      var owner = (this.wrapper || this).nextSibling;

      // If there isn't an offset parent, we have nothing to do.
      if (!owner.offsetParent)
        return;

      // Position the bubble below the location of the owner.
      var left = owner.offsetLeft + owner.offsetWidth / 2 -
          this.offsetWidth / 2;

      var BUBBLE_EDGE_OFFSET = 5;
      var top = owner.offsetTop;
      if (this.innards_.classList.contains('above'))
        top -= this.offsetHeight + BUBBLE_EDGE_OFFSET;
      else
        top += owner.offsetHeight + BUBBLE_EDGE_OFFSET;

      // Update the position in the CSS.  Cache the last values for
      // best performance.
      if (left != this.lastLeft) {
        this.style.left = left + 'px';
        this.lastLeft = left;
      }
      if (top != this.lastTop) {
        this.style.top = top + 'px';
        this.lastTop = top;
      }
    },
  };

  /**
   * Encapsulated handling of the search page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function SearchPage() {
    Page.call(this, 'search',
              loadTimeData.getString('searchPageTabTitle'),
              'searchPage');
  }

  cr.addSingletonGetter(SearchPage);

  SearchPage.prototype = {
    // Inherit SearchPage from Page.
    __proto__: Page.prototype,

    /**
     * Wait a bit to see if the user is still entering search text.
     * @type {number|undefined}
     * @private
     */
    delayedSearchMetric_: undefined,

    /**
     * Only send the time of first search once.
     * @type {boolean}
     * @private
     */
    hasSentFirstSearchTime_: false,

    /**
     * A boolean to prevent recursion. Used by setSearchText_().
     * @type {boolean}
     * @private
     */
    insideSetSearchText_: false,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      // Record the start time for use in reporting metrics.
      this.createdTimestamp_ = Date.now();

      this.searchField = $('search-field');

      // Handle search events. (No need to throttle, WebKit's search field
      // will do that automatically.)
      this.searchField.onsearch = function(e) {
        this.setSearchText_(e.currentTarget.value);
      }.bind(this);

      // Install handler for key presses.
      document.addEventListener('keydown',
                                this.keyDownEventHandler_.bind(this));
    },

    /** @override */
    get sticky() {
      return true;
    },

    /** @override */
    didShowPage: function() {
      // This method is called by the PageManager after all pages have had their
      // visibility attribute set. At this point we can perform the
      // search-specific DOM manipulation.
      this.setSearchActive_(true);
    },

    /** @override */
    didChangeHash: function() {
      this.setSearchActive_(true);
    },

    /** @override */
    willHidePage: function() {
      // This method is called by the PageManager before all pages have their
      // visibility attribute set. Before that happens, we need to undo the
      // search-specific DOM manipulation that was performed in didShowPage.
      this.setSearchActive_(false);
    },

    /**
     * Update the UI to reflect whether we are in a search state.
     * @param {boolean} active True if we are on the search page.
     * @private
     */
    setSearchActive_: function(active) {
      // It's fine to exit if search wasn't active and we're not going to
      // activate it now.
      if (!this.searchActive_ && !active)
        return;

      if (!ShouldEnableSearch())
        return;

      this.searchActive_ = active;

      if (active) {
        var hash = this.hash;
        if (hash) {
          this.searchField.value =
              decodeURIComponent(hash.slice(1).replace(/\+/g, ' '));
        } else if (!this.searchField.value) {
          // This should only happen if the user goes directly to
          // chrome://settings-frame/search
          PageManager.showDefaultPage();
          return;
        }

        // Move 'advanced' sections into the main settings page to allow
        // searching.
        if (!this.advancedSections_) {
          this.advancedSections_ =
              $('advanced-settings-container').querySelectorAll('section');
          for (var i = 0, section; section = this.advancedSections_[i]; i++)
            $('settings').appendChild(section);
        }
      } else {
        this.searchField.value = '';
      }

      var pagesToSearch = this.getSearchablePages_();
      for (var i = 0; i < pagesToSearch.length; i++) {
        var page = pagesToSearch[i];

        if (!active)
          page.visible = false;

        // Update the visible state of all top-level elements that are not
        // sections (ie titles, button strips).  We do this before changing
        // the page visibility to avoid excessive re-draw.
        for (var i = 0, childDiv; childDiv = page.pageDiv.children[i]; i++) {
          if (active) {
            if (childDiv.tagName != 'SECTION')
              childDiv.classList.add('search-hidden');
          } else {
            childDiv.classList.remove('search-hidden');
          }
        }

        if (active) {
          // When search is active, remove the 'hidden' tag.  This tag may have
          // been added by the PageManager.
          page.pageDiv.hidden = false;
        }
      }

      if (active) {
        this.setSearchText_(this.searchField.value);
        this.searchField.focus();
      } else {
        // After hiding all page content, remove any search results.
        this.unhighlightMatches_();
        this.removeSearchBubbles_();

        // Move 'advanced' sections back into their original container.
        if (this.advancedSections_) {
          for (var i = 0, section; section = this.advancedSections_[i]; i++)
            $('advanced-settings-container').appendChild(section);
          this.advancedSections_ = null;
        }
      }
    },

    /**
     * Set the current search criteria.
     * @param {string} text Search text.
     * @private
     */
    setSearchText_: function(text) {
      if (!ShouldEnableSearch())
        return;

      // Prevent recursive execution of this method.
      if (this.insideSetSearchText_) return;
      this.insideSetSearchText_ = true;

      // Cleanup the search query string.
      text = SearchPage.canonicalizeQuery(text);

      // If the search string becomes empty, flip back to the default page.
      if (!text) {
        if (this.searchActive_)
          PageManager.showDefaultPage();
        this.insideSetSearchText_ = false;
        return;
      }

      if (!this.hasSentFirstSearchTime_) {
        this.hasSentFirstSearchTime_ = true;
        chrome.metricsPrivate.recordMediumTime('Settings.TimeToFirstSearch',
            Date.now() - this.createdTimestamp_);
      }

      // Toggle the search page if necessary. Otherwise, update the hash.
      var hash = '#' + encodeURIComponent(text);
      if (this.searchActive_) {
        if (this.hash != hash)
          this.setHash(hash);
      } else {
        PageManager.showPageByName(this.name, true, {hash: hash});
      }

      var foundMatches = false;

      // Remove any prior search results.
      this.unhighlightMatches_();
      this.removeSearchBubbles_();

      var pagesToSearch = this.getSearchablePages_();
      for (var i = 0; i < pagesToSearch.length; i++) {
        var page = pagesToSearch[i];
        var elements = page.pageDiv.querySelectorAll('section');
        for (var i = 0, node; node = elements[i]; i++) {
          node.classList.add('search-hidden');
        }
      }

      var bubbleControls = [];
      var pageMatchesForMetrics = 0;
      var subpageMatchesForMetrics = 0;
      var sectionMatchesForMetrics = {};

      // Generate search text by applying lowercase and escaping any characters
      // that would be problematic for regular expressions.
      var searchText =
          text.toLowerCase().replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
      // Generate a regular expression for hilighting search terms.
      var regExp = new RegExp('(' + searchText + ')', 'ig');

      if (searchText.length) {
        // Search all top-level sections for anchored string matches.
        for (var i = 0; i < pagesToSearch.length; i++) {
          var page = pagesToSearch[i];
          var elements =
              page.pageDiv.querySelectorAll('section');
          for (var i = 0, node; node = elements[i]; i++) {
            if (this.highlightMatches_(regExp, node)) {
              node.classList.remove('search-hidden');
              if (!node.hidden) {
                foundMatches = true;
                pageMatchesForMetrics += 1;
                var section = SettingsSections[node.id] ||
                    SettingsSections['Unknown'];
                sectionMatchesForMetrics[section] = section;
              }
            }
          }
        }

        // Search all sub-pages, generating an array of top-level sections that
        // we need to make visible.
        var subPagesToSearch = this.getSearchableSubPages_();
        var control, node;
        for (var i = 0; i < subPagesToSearch.length; i++) {
          var page = subPagesToSearch[i];
          if (this.highlightMatches_(regExp, page.pageDiv)) {
            this.revealAssociatedSections_(page);

            bubbleControls =
                bubbleControls.concat(this.getAssociatedControls_(page));

            foundMatches = true;
            subpageMatchesForMetrics += 1;
          }
        }
      }

      // Configure elements on the search results page based on search results.
      $('searchPageNoMatches').hidden = foundMatches;

      // Create search balloons for sub-page results.
      var bubbleCount = bubbleControls.length;
      for (var i = 0; i < bubbleCount; i++)
        this.createSearchBubble_(bubbleControls[i], text);

      // If the search doesn't change for one second, send some metrics.
      clearTimeout(this.delayedSearchMetric_);
      this.delayedSearchMetric_ = setTimeout(function() {
        if (!foundMatches) {
          chrome.metricsPrivate.recordSmallCount(
            'Settings.SearchLengthNoMatch', text.length);
          chrome.metricsPrivate.recordSmallCount(
            'Settings.SearchSections', SettingsSections['None']);
        } else {
          for (var section in sectionMatchesForMetrics) {
            var sectionId = sectionMatchesForMetrics[section];
            assert(sectionId !== undefined);
            chrome.metricsPrivate.recordSmallCount(
              'Settings.SearchSections', sectionId);
          }
        }

        chrome.metricsPrivate.recordUserAction('Settings.Searching');
        chrome.metricsPrivate.recordSmallCount(
            'Settings.SearchLength', text.length);
        chrome.metricsPrivate.recordSmallCount(
            'Settings.SearchPageMatchCount', pageMatchesForMetrics);
        chrome.metricsPrivate.recordSmallCount(
            'Settings.SearchSubpageMatchCount', subpageMatchesForMetrics);
      }, 1000);

      // Cleanup the recursion-prevention variable.
      this.insideSetSearchText_ = false;
    },

    /**
     * Reveal the associated section for |subpage|, as well as the one for its
     * |parentPage|, and its |parentPage|'s |parentPage|, etc.
     * @private
     */
    revealAssociatedSections_: function(subpage) {
      for (var page = subpage; page; page = page.parentPage) {
        var section = page.associatedSection;
        if (section)
          section.classList.remove('search-hidden');
      }
    },

    /**
     * @return {!Array<HTMLElement>} all the associated controls for |subpage|,
     * including |subpage.associatedControls| as well as any controls on parent
     * pages that are indirectly necessary to get to the subpage.
     * @private
     */
    getAssociatedControls_: function(subpage) {
      var controls = [];
      for (var page = subpage; page; page = page.parentPage) {
        if (page.associatedControls)
          controls = controls.concat(page.associatedControls);
      }
      return controls;
    },

    /**
     * Wraps matches in spans.
     * @param {RegExp} regExp The search query (in regexp form).
     * @param {Element} element An HTML container element to recursively search
     *     within.
     * @return {boolean} true if the element was changed.
     * @private
     */
    highlightMatches_: function(regExp, element) {
      var found = false;
      var div, child, tmp;

      // Walk the tree, searching each TEXT node.
      var walker = document.createTreeWalker(element,
                                             NodeFilter.SHOW_TEXT,
                                             null,
                                             false);
      var node = walker.nextNode();
      while (node) {
        var textContent = node.nodeValue;
        // Perform a search and replace on the text node value.
        var split = textContent.split(regExp);
        if (split.length > 1) {
          found = true;
          var nextNode = walker.nextNode();
          var parentNode = node.parentNode;
          // Use existing node as placeholder to determine where to insert the
          // replacement content.
          for (var i = 0; i < split.length; ++i) {
            if (i % 2 == 0) {
              parentNode.insertBefore(document.createTextNode(split[i]), node);
            } else {
              var span = document.createElement('span');
              span.className = 'search-highlighted';
              span.textContent = split[i];
              parentNode.insertBefore(span, node);
            }
          }
          // Remove old node.
          parentNode.removeChild(node);
          node = nextNode;
        } else {
          node = walker.nextNode();
        }
      }

      return found;
    },

    /**
     * Removes all search highlight tags from the document.
     * @private
     */
    unhighlightMatches_: function() {
      // Find all search highlight elements.
      var elements = document.querySelectorAll('.search-highlighted');

      // For each element, remove the highlighting.
      var parent, i;
      for (var i = 0, node; node = elements[i]; i++) {
        parent = node.parentNode;

        // Replace the highlight element with the first child (the text node).
        parent.replaceChild(node.firstChild, node);

        // Normalize the parent so that multiple text nodes will be combined.
        parent.normalize();
      }
    },

    /**
     * Creates a search result bubble attached to an element.
     * @param {Element} element An HTML element, usually a button.
     * @param {string} text A string to show in the bubble.
     * @private
     */
    createSearchBubble_: function(element, text) {
      // avoid appending multiple bubbles to a button.
      var sibling = element.previousElementSibling;
      if (sibling && (sibling.classList.contains('search-bubble') ||
                      sibling.classList.contains('search-bubble-wrapper')))
        return;

      var parent = element.parentElement;
      if (parent) {
        var bubble = new SearchBubble(text);
        bubble.attachTo(element);
        bubble.updatePosition();
      }
    },

    /**
     * Removes all search match bubbles.
     * @private
     */
    removeSearchBubbles_: function() {
      var elements = document.querySelectorAll('.search-bubble');
      var length = elements.length;
      for (var i = 0; i < length; i++)
        elements[i].dispose();
    },

    /**
     * Builds a list of top-level pages to search.  Omits the search page and
     * all sub-pages.
     * @return {Array} An array of pages to search.
     * @private
     */
    getSearchablePages_: function() {
      var name, page, pages = [];
      for (name in PageManager.registeredPages) {
        if (name != this.name) {
          page = PageManager.registeredPages[name];
          if (!page.parentPage)
            pages.push(page);
        }
      }
      return pages;
    },

    /**
     * Builds a list of sub-pages (and overlay pages) to search.  Ignore pages
     * that have no associated controls, or whose controls are hidden.
     * @return {Array} An array of pages to search.
     * @private
     */
    getSearchableSubPages_: function() {
      var name, pageInfo, page, pages = [];
      for (name in PageManager.registeredPages) {
        page = PageManager.registeredPages[name];
        if (page.parentPage &&
            page.associatedSection &&
            !page.associatedSection.hidden) {
          pages.push(page);
        }
      }
      for (name in PageManager.registeredOverlayPages) {
        page = PageManager.registeredOverlayPages[name];
        if (page.associatedSection &&
            !page.associatedSection.hidden &&
            page.pageDiv != undefined) {
          pages.push(page);
        }
      }
      return pages;
    },

    /**
     * A function to handle key press events.
     * @param {Event} event A keydown event.
     * @private
     */
    keyDownEventHandler_: function(event) {
      /** @const */ var ESCAPE_KEY_CODE = 27;
      /** @const */ var FORWARD_SLASH_KEY_CODE = 191;

      switch (event.keyCode) {
        case ESCAPE_KEY_CODE:
          if (event.target == this.searchField) {
            this.setSearchText_('');
            this.searchField.blur();
            event.stopPropagation();
            event.preventDefault();
          }
          break;
        case FORWARD_SLASH_KEY_CODE:
          if (!/INPUT|SELECT|BUTTON|TEXTAREA/.test(event.target.tagName) &&
              !event.ctrlKey && !event.altKey) {
            this.searchField.focus();
            event.stopPropagation();
            event.preventDefault();
          }
          break;
      }
    },
  };

  /**
   * Standardizes a user-entered text query by removing extra whitespace.
   * @param {string} text The user-entered text.
   * @return {string} The trimmed query.
   */
  SearchPage.canonicalizeQuery = function(text) {
    // Trim beginning and ending whitespace.
    return text.replace(/^\s+|\s+$/g, '');
  };

  // Export
  return {
    SearchPage: SearchPage
  };

});
