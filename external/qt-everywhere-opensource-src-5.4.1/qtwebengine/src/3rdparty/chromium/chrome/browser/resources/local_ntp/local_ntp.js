// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview The local InstantExtended NTP.
 */


/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
<include src="../../../../ui/webui/resources/js/assert.js">
<include src="window_disposition_util.js">


/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
var CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo', // Shows white logo if required by theme
  BLACKLIST: 'mv-blacklist', // triggers tile blacklist animation
  BLACKLIST_BUTTON: 'mv-x',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  FAKEBOX_DISABLE: 'fakebox-disable', // Makes fakebox non-interactive
  FAKEBOX_FOCUS: 'fakebox-focused', // Applies focus styles to the fakebox
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  FAVICON: 'mv-favicon',
  HIDE_BLACKLIST_BUTTON: 'mv-x-hide', // hides blacklist button during animation
  HIDE_FAKEBOX_AND_LOGO: 'hide-fakebox-logo',
  HIDE_NOTIFICATION: 'mv-notice-hide',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  PAGE: 'mv-page', // page tiles
  PAGE_READY: 'mv-page-ready',  // page tile when ready
  ROW: 'mv-row',  // tile row
  RTL: 'rtl',  // Right-to-left language text.
  THUMBNAIL: 'mv-thumb',
  THUMBNAIL_MASK: 'mv-mask',
  TILE: 'mv-tile',
  TITLE: 'mv-title'
};


/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
var IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
  CUSTOM_THEME_STYLE: 'ct-style',
  FAKEBOX: 'fakebox',
  FAKEBOX_INPUT: 'fakebox-input',
  LOGO: 'logo',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CLOSE_BUTTON: 'mv-notice-x',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  RESTORE_ALL_LINK: 'mv-restore',
  TILES: 'mv-tiles',
  UNDO_LINK: 'mv-undo'
};


/**
 * Enum for keycodes.
 * @enum {number}
 * @const
 */
var KEYCODE = {
  DELETE: 46,
  ENTER: 13
};


/**
 * Enum for the state of the NTP when it is disposed.
 * @enum {number}
 * @const
 */
var NTP_DISPOSE_STATE = {
  NONE: 0,  // Preserve the NTP appearance and functionality
  DISABLE_FAKEBOX: 1,
  HIDE_FAKEBOX_AND_LOGO: 2
};


/**
 * The JavaScript button event value for a middle click.
 * @type {number}
 * @const
 */
var MIDDLE_MOUSE_BUTTON = 1;


/**
 * The container for the tile elements.
 * @type {Element}
 */
var tilesContainer;


/**
 * The notification displayed when a page is blacklisted.
 * @type {Element}
 */
var notification;


/**
 * The container for the theme attribution.
 * @type {Element}
 */
var attribution;


/**
 * The "fakebox" - an input field that looks like a regular searchbox.  When it
 * is focused, any text the user types goes directly into the omnibox.
 * @type {Element}
 */
var fakebox;


/**
 * The container for NTP elements.
 * @type {Element}
 */
var ntpContents;


/**
 * The array of rendered tiles, ordered by appearance.
 * @type {!Array.<Tile>}
 */
var tiles = [];


/**
 * The last blacklisted tile if any, which by definition should not be filler.
 * @type {?Tile}
 */
var lastBlacklistedTile = null;


/**
 * True if a page has been blacklisted and we're waiting on the
 * onmostvisitedchange callback. See onMostVisitedChange() for how this
 * is used.
 * @type {boolean}
 */
var isBlacklisting = false;


/**
 * Current number of tiles columns shown based on the window width, including
 * those that just contain filler.
 * @type {number}
 */
var numColumnsShown = 0;


/**
 * True if the user initiated the current most visited change and false
 * otherwise.
 * @type {boolean}
 */
var userInitiatedMostVisitedChange = false;


/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
var ntpApiHandle;


/**
 * The browser embeddedSearch.searchBox object.
 * @type {Object}
 */
var searchboxApiHandle;


/**
 * The state of the NTP when a query is entered into the Omnibox.
 * @type {NTP_DISPOSE_STATE}
 */
var omniboxInputBehavior = NTP_DISPOSE_STATE.NONE;


/**
 * The state of the NTP when a query is entered into the Fakebox.
 * @type {NTP_DISPOSE_STATE}
 */
var fakeboxInputBehavior = NTP_DISPOSE_STATE.HIDE_FAKEBOX_AND_LOGO;


/**
 * Total tile width. Should be equal to mv-tile's width + 2 * border-width.
 * @private {number}
 * @const
 */
var TILE_WIDTH = 140;


/**
 * Margin between tiles. Should be equal to mv-tile's -webkit-margin-start.
 * @private {number}
 * @const
 */
var TILE_MARGIN_START = 20;


/** @type {number} @const */
var MAX_NUM_TILES_TO_SHOW = 8;


/** @type {number} @const */
var MIN_NUM_COLUMNS = 2;


/** @type {number} @const */
var MAX_NUM_COLUMNS = 4;


/** @type {number} @const */
var NUM_ROWS = 2;


/**
 * Minimum total padding to give to the left and right of the most visited
 * section. Used to determine how many tiles to show.
 * @type {number}
 * @const
 */
var MIN_TOTAL_HORIZONTAL_PADDING = 200;


/**
 * The filename for a most visited iframe src which shows a page title.
 * @type {string}
 * @const
 */
var MOST_VISITED_TITLE_IFRAME = 'title.html';


/**
 * The filename for a most visited iframe src which shows a thumbnail image.
 * @type {string}
 * @const
 */
var MOST_VISITED_THUMBNAIL_IFRAME = 'thumbnail.html';


/**
 * The hex color for most visited tile elements.
 * @type {string}
 * @const
 */
var MOST_VISITED_COLOR = '777777';


/**
 * The font family for most visited tile elements.
 * @type {string}
 * @const
 */
var MOST_VISITED_FONT_FAMILY = 'arial, sans-serif';


/**
 * The font size for most visited tile elements.
 * @type {number}
 * @const
 */
var MOST_VISITED_FONT_SIZE = 11;


/**
 * Hide most visited tiles for at most this many milliseconds while painting.
 * @type {number}
 * @const
 */
var MOST_VISITED_PAINT_TIMEOUT_MSEC = 500;


/**
 * A Tile is either a rendering of a Most Visited page or "filler" used to
 * pad out the section when not enough pages exist.
 *
 * @param {Element} elem The element for rendering the tile.
 * @param {number=} opt_rid The RID for the corresponding Most Visited page.
 *     Should only be left unspecified when creating a filler tile.
 * @constructor
 */
function Tile(elem, opt_rid) {
  /** @type {Element} */
  this.elem = elem;

  /** @type {number|undefined} */
  this.rid = opt_rid;
}


/**
 * Updates the NTP based on the current theme.
 * @private
 */
function onThemeChange() {
  var info = ntpApiHandle.themeBackgroundInfo;
  if (!info)
    return;

  var background = [convertToRGBAColor(info.backgroundColorRgba),
                    info.imageUrl,
                    info.imageTiling,
                    info.imageHorizontalAlignment,
                    info.imageVerticalAlignment].join(' ').trim();
  document.body.style.background = background;
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, info.alternateLogo);
  updateThemeAttribution(info.attributionUrl);
  setCustomThemeStyle(info);
  renderTiles();
}


/**
 * Updates the NTP style according to theme.
 * @param {Object=} opt_themeInfo The information about the theme. If it is
 * omitted the style will be reverted to the default.
 * @private
 */
function setCustomThemeStyle(opt_themeInfo) {
  var customStyleElement = $(IDS.CUSTOM_THEME_STYLE);
  var head = document.head;

  if (opt_themeInfo && !opt_themeInfo.usingDefaultTheme) {
    var themeStyle =
      '#attribution {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-msg {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorRgba) + ';' +
      '}' +
      '#mv-notice-links span {' +
      '  color: ' + convertToRGBAColor(opt_themeInfo.textColorLightRgba) + ';' +
      '}' +
      '#mv-notice-x {' +
      '  -webkit-filter: drop-shadow(0 0 0 ' +
          convertToRGBAColor(opt_themeInfo.textColorRgba) + ');' +
      '}' +
      '.mv-page-ready {' +
      '  border: 1px solid ' +
        convertToRGBAColor(opt_themeInfo.sectionBorderColorRgba) + ';' +
      '}' +
      '.mv-page-ready:hover, .mv-page-ready:focus {' +
      '  border-color: ' +
          convertToRGBAColor(opt_themeInfo.headerColorRgba) + ';' +
      '}';

    if (customStyleElement) {
      customStyleElement.textContent = themeStyle;
    } else {
      customStyleElement = document.createElement('style');
      customStyleElement.type = 'text/css';
      customStyleElement.id = IDS.CUSTOM_THEME_STYLE;
      customStyleElement.textContent = themeStyle;
      head.appendChild(customStyleElement);
    }

  } else if (customStyleElement) {
    head.removeChild(customStyleElement);
  }
}


/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string} url The URL of the attribution image, if any.
 * @private
 */
function updateThemeAttribution(url) {
  if (!url) {
    setAttributionVisibility_(false);
    return;
  }

  var attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;
  setAttributionVisibility_(true);
}


/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 * @private
 */
function setAttributionVisibility_(show) {
  if (attribution) {
    attribution.style.display = show ? '' : 'none';
  }
}


 /**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array.<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 * @private
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
                    color[3] / 255 + ')';
}


/**
 * Handles a new set of Most Visited page data.
 */
function onMostVisitedChange() {
  var pages = ntpApiHandle.mostVisited;

  if (isBlacklisting) {
    // Trigger the blacklist animation and re-render the tiles when it
    // completes.
    var lastBlacklistedTileElement = lastBlacklistedTile.elem;
    lastBlacklistedTileElement.addEventListener(
        'webkitTransitionEnd', blacklistAnimationDone);
    lastBlacklistedTileElement.classList.add(CLASSES.BLACKLIST);

  } else {
    // Otherwise render the tiles using the new data without animation.
    tiles = [];
    for (var i = 0; i < MAX_NUM_TILES_TO_SHOW; ++i) {
      tiles.push(createTile(pages[i], i));
    }
    if (!userInitiatedMostVisitedChange) {
      tilesContainer.hidden = true;
      window.setTimeout(function() {
        if (tilesContainer) {
          tilesContainer.hidden = false;
        }
      }, MOST_VISITED_PAINT_TIMEOUT_MSEC);
    }
    renderTiles();
  }
}


/**
 * Renders the current set of tiles.
 */
function renderTiles() {
  var rows = tilesContainer.children;
  for (var i = 0; i < rows.length; ++i) {
    removeChildren(rows[i]);
  }

  for (var i = 0, length = tiles.length;
       i < Math.min(length, numColumnsShown * NUM_ROWS); ++i) {
    rows[Math.floor(i / numColumnsShown)].appendChild(tiles[i].elem);
  }
}


/**
 * Shows most visited tiles if all child iframes are loaded, and hides them
 * otherwise.
 */
function updateMostVisitedVisibility() {
  var iframes = tilesContainer.querySelectorAll('iframe');
  var ready = true;
  for (var i = 0, numIframes = iframes.length; i < numIframes; i++) {
    if (iframes[i].hidden) {
      ready = false;
      break;
    }
  }
  if (ready) {
    tilesContainer.hidden = false;
    userInitiatedMostVisitedChange = false;
  }
}


/**
 * Builds a URL to display a most visited tile component in an iframe.
 * @param {string} filename The desired most visited component filename.
 * @param {number} rid The restricted ID.
 * @param {string} color The text color for text in the iframe.
 * @param {string} fontFamily The font family for text in the iframe.
 * @param {number} fontSize The font size for text in the iframe.
 * @param {number} position The position of the iframe in the UI.
 * @return {string} An URL to display the most visited component in an iframe.
 */
function getMostVisitedIframeUrl(filename, rid, color, fontFamily, fontSize,
    position) {
  return 'chrome-search://most-visited/' + encodeURIComponent(filename) + '?' +
      ['rid=' + encodeURIComponent(rid),
       'c=' + encodeURIComponent(color),
       'f=' + encodeURIComponent(fontFamily),
       'fs=' + encodeURIComponent(fontSize),
       'pos=' + encodeURIComponent(position)].join('&');
}


/**
 * Creates a Tile with the specified page data. If no data is provided, a
 * filler Tile is created.
 * @param {Object} page The page data.
 * @param {number} position The position of the tile.
 * @return {Tile} The new Tile.
 */
function createTile(page, position) {
  var tileElement = document.createElement('div');
  tileElement.classList.add(CLASSES.TILE);

  if (page) {
    var rid = page.rid;
    tileElement.classList.add(CLASSES.PAGE);

    var navigateFunction = function(e) {
      e.preventDefault();
      ntpApiHandle.navigateContentWindow(rid, getDispositionFromEvent(e));
    };

    // The click handler for navigating to the page identified by the RID.
    tileElement.addEventListener('click', navigateFunction);

    // Make thumbnails tab-accessible.
    tileElement.setAttribute('tabindex', '1');
    registerKeyHandler(tileElement, KEYCODE.ENTER, navigateFunction);

    // The iframe which renders the page title.
    var titleElement = document.createElement('iframe');
    titleElement.tabIndex = '-1';

    // Why iframes have IDs:
    //
    // On navigating back to the NTP we see several onmostvisitedchange() events
    // in series with incrementing RIDs. After the first event, a set of iframes
    // begins loading RIDs n, n+1, ..., n+k-1; after the second event, these get
    // destroyed and a new set begins loading RIDs n+k, n+k+1, ..., n+2k-1.
    // Now due to crbug.com/68841, Chrome incorrectly loads the content for the
    // first set of iframes into the most recent set of iframes.
    //
    // Giving iframes distinct ids seems to cause some invalidation and prevent
    // associating the incorrect data.
    //
    // TODO(jered): Find and fix the root (probably Blink) bug.

    titleElement.src = getMostVisitedIframeUrl(
        MOST_VISITED_TITLE_IFRAME, rid, MOST_VISITED_COLOR,
        MOST_VISITED_FONT_FAMILY, MOST_VISITED_FONT_SIZE, position);

    // Keep this id here. See comment above.
    titleElement.id = 'title-' + rid;
    titleElement.hidden = true;
    titleElement.onload = function() {
      titleElement.hidden = false;
      updateMostVisitedVisibility();
    };
    titleElement.className = CLASSES.TITLE;
    tileElement.appendChild(titleElement);

    // The iframe which renders either a thumbnail or domain element.
    var thumbnailElement = document.createElement('iframe');
    thumbnailElement.tabIndex = '-1';
    thumbnailElement.src = getMostVisitedIframeUrl(
        MOST_VISITED_THUMBNAIL_IFRAME, rid, MOST_VISITED_COLOR,
        MOST_VISITED_FONT_FAMILY, MOST_VISITED_FONT_SIZE, position);

    // Keep this id here. See comment above.
    thumbnailElement.id = 'thumb-' + rid;
    thumbnailElement.hidden = true;
    thumbnailElement.onload = function() {
      thumbnailElement.hidden = false;
      tileElement.classList.add(CLASSES.PAGE_READY);
      updateMostVisitedVisibility();
    };
    thumbnailElement.className = CLASSES.THUMBNAIL;
    tileElement.appendChild(thumbnailElement);

    // A mask to darken the thumbnail on focus.
    var maskElement = createAndAppendElement(
        tileElement, 'div', CLASSES.THUMBNAIL_MASK);

    // The button used to blacklist this page.
    var blacklistButton = createAndAppendElement(
        tileElement, 'div', CLASSES.BLACKLIST_BUTTON);
    var blacklistFunction = generateBlacklistFunction(rid);
    blacklistButton.addEventListener('click', blacklistFunction);
    blacklistButton.title = configData.translatedStrings.removeThumbnailTooltip;

    // When a tile is focused, have delete also blacklist the page.
    registerKeyHandler(tileElement, KEYCODE.DELETE, blacklistFunction);

    // The page favicon, if any.
    var faviconUrl = page.faviconUrl;
    if (faviconUrl) {
      var favicon = createAndAppendElement(
          tileElement, 'div', CLASSES.FAVICON);
      favicon.style.backgroundImage = 'url(' + faviconUrl + ')';
    }
    return new Tile(tileElement, rid);
  } else {
    return new Tile(tileElement);
  }
}


/**
 * Generates a function to be called when the page with the corresponding RID
 * is blacklisted.
 * @param {number} rid The RID of the page being blacklisted.
 * @return {function(Event)} A function which handles the blacklisting of the
 *     page by updating state variables and notifying Chrome.
 */
function generateBlacklistFunction(rid) {
  return function(e) {
    // Prevent navigation when the page is being blacklisted.
    e.stopPropagation();

    userInitiatedMostVisitedChange = true;
    isBlacklisting = true;
    tilesContainer.classList.add(CLASSES.HIDE_BLACKLIST_BUTTON);
    lastBlacklistedTile = getTileByRid(rid);
    ntpApiHandle.deleteMostVisitedItem(rid);
  };
}


/**
 * Shows the blacklist notification and triggers a delay to hide it.
 */
function showNotification() {
  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  notification.classList.remove(CLASSES.DELAYED_HIDE_NOTIFICATION);
  notification.scrollTop;
  notification.classList.add(CLASSES.DELAYED_HIDE_NOTIFICATION);
}


/**
 * Hides the blacklist notification.
 */
function hideNotification() {
  notification.classList.add(CLASSES.HIDE_NOTIFICATION);
}


/**
 * Handles the end of the blacklist animation by showing the notification and
 * re-rendering the new set of tiles.
 */
function blacklistAnimationDone() {
  showNotification();
  isBlacklisting = false;
  tilesContainer.classList.remove(CLASSES.HIDE_BLACKLIST_BUTTON);
  lastBlacklistedTile.elem.removeEventListener(
      'webkitTransitionEnd', blacklistAnimationDone);
  // Need to call explicitly to re-render the tiles, since the initial
  // onmostvisitedchange issued by the blacklist function only triggered
  // the animation.
  onMostVisitedChange();
}


/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  userInitiatedMostVisitedChange = true;
  hideNotification();
  var lastBlacklistedRID = lastBlacklistedTile.rid;
  if (typeof lastBlacklistedRID != 'undefined')
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedRID);
}


/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  userInitiatedMostVisitedChange = true;
  hideNotification();
  ntpApiHandle.undoAllMostVisitedDeletions();
}


/**
 * Re-renders the tiles if the number of columns has changed.  As a temporary
 * fix for crbug/240510, updates the width of the fakebox and most visited tiles
 * container.
 */
function onResize() {
  // If innerWidth is zero, then use the maximum snap size.
  var innerWidth = window.innerWidth || 820;

  // These values should remain in sync with local_ntp.css.
  // TODO(jeremycho): Delete once the root cause of crbug/240510 is resolved.
  var setWidths = function(tilesContainerWidth) {
    tilesContainer.style.width = tilesContainerWidth + 'px';
    if (fakebox)
      fakebox.style.width = (tilesContainerWidth - 2) + 'px';
  };
  if (innerWidth >= 820)
    setWidths(620);
  else if (innerWidth >= 660)
    setWidths(460);
  else
    setWidths(300);

  var tileRequiredWidth = TILE_WIDTH + TILE_MARGIN_START;
  // Adds margin-start to the available width to compensate the extra margin
  // counted above for the first tile (which does not have a margin-start).
  var availableWidth = innerWidth + TILE_MARGIN_START -
      MIN_TOTAL_HORIZONTAL_PADDING;
  var numColumnsToShow = Math.floor(availableWidth / tileRequiredWidth);
  numColumnsToShow = Math.max(MIN_NUM_COLUMNS,
                              Math.min(MAX_NUM_COLUMNS, numColumnsToShow));
  if (numColumnsToShow != numColumnsShown) {
    numColumnsShown = numColumnsToShow;
    renderTiles();
  }
}


/**
 * Returns the tile corresponding to the specified page RID.
 * @param {number} rid The page RID being looked up.
 * @return {Tile} The corresponding tile.
 */
function getTileByRid(rid) {
  for (var i = 0, length = tiles.length; i < length; ++i) {
    var tile = tiles[i];
    if (tile.rid == rid)
      return tile;
  }
  return null;
}


/**
 * Handles new input by disposing the NTP, according to where the input was
 * entered.
 */
function onInputStart() {
  if (fakebox && isFakeboxFocused()) {
    setFakeboxFocus(false);
    setFakeboxDragFocus(false);
    disposeNtp(true);
  } else if (!isFakeboxFocused()) {
    disposeNtp(false);
  }
}


/**
 * Disposes the NTP, according to where the input was entered.
 * @param {boolean} wasFakeboxInput True if the input was in the fakebox.
 */
function disposeNtp(wasFakeboxInput) {
  var behavior = wasFakeboxInput ? fakeboxInputBehavior : omniboxInputBehavior;
  if (behavior == NTP_DISPOSE_STATE.DISABLE_FAKEBOX)
    setFakeboxActive(false);
  else if (behavior == NTP_DISPOSE_STATE.HIDE_FAKEBOX_AND_LOGO)
    setFakeboxAndLogoVisibility(false);
}


/**
 * Restores the NTP (re-enables the fakebox and unhides the logo.)
 */
function restoreNtp() {
  setFakeboxActive(true);
  setFakeboxAndLogoVisibility(true);
}


/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} focus True to show a dragging focus to the fakebox.
 */
function setFakeboxDragFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DRAG_FOCUS, focus);
}

/**
 * @return {boolean} True if the fakebox has focus.
 */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS) ||
      document.body.classList.contains(CLASSES.FAKEBOX_DRAG_FOCUS);
}


/**
 * @param {boolean} enable True to enable the fakebox.
 */
function setFakeboxActive(enable) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DISABLE, !enable);
}


/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return fakebox.contains(event.target) &&
      !document.body.classList.contains(CLASSES.FAKEBOX_DISABLE);
}


/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxAndLogoVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX_AND_LOGO, !show);
}


/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}


/**
 * Utility function which creates an element with an optional classname and
 * appends it to the specified parent.
 * @param {Element} parent The parent to append the new element.
 * @param {string} name The name of the new element.
 * @param {string=} opt_class The optional classname of the new element.
 * @return {Element} The new element.
 */
function createAndAppendElement(parent, name, opt_class) {
  var child = document.createElement(name);
  if (opt_class)
    child.classList.add(opt_class);
  parent.appendChild(child);
  return child;
}


/**
 * Removes a node from its parent.
 * @param {Node} node The node to remove.
 */
function removeNode(node) {
  node.parentNode.removeChild(node);
}


/**
 * Removes all the child nodes on a DOM node.
 * @param {Node} node Node to remove children from.
 */
function removeChildren(node) {
  node.innerHTML = '';
}


/**
 * @param {!Element} element The element to register the handler for.
 * @param {number} keycode The keycode of the key to register.
 * @param {!Function} handler The key handler to register.
 */
function registerKeyHandler(element, keycode, handler) {
  element.addEventListener('keydown', function(event) {
    if (event.keyCode == keycode)
      handler(event);
  });
}


/**
 * @return {Object} the handle to the embeddedSearch API.
 */
function getEmbeddedSearchApiHandle() {
  if (window.cideb)
    return window.cideb;
  if (window.chrome && window.chrome.embeddedSearch)
    return window.chrome.embeddedSearch;
  return null;
}


/**
 * Prepares the New Tab Page by adding listeners, rendering the current
 * theme, the most visited pages section, and Google-specific elements for a
 * Google-provided page.
 */
function init() {
  tilesContainer = $(IDS.TILES);
  notification = $(IDS.NOTIFICATION);
  attribution = $(IDS.ATTRIBUTION);
  ntpContents = $(IDS.NTP_CONTENTS);

  for (var i = 0; i < NUM_ROWS; i++) {
    var row = document.createElement('div');
    row.classList.add(CLASSES.ROW);
    tilesContainer.appendChild(row);
  }

  if (configData.isGooglePage) {
    var logo = document.createElement('div');
    logo.id = IDS.LOGO;

    fakebox = document.createElement('div');
    fakebox.id = IDS.FAKEBOX;
    fakebox.innerHTML =
        '<input id="' + IDS.FAKEBOX_INPUT +
            '" autocomplete="off" tabindex="-1" aria-hidden="true">' +
        '<div id=cursor></div>';

    ntpContents.insertBefore(fakebox, ntpContents.firstChild);
    ntpContents.insertBefore(logo, ntpContents.firstChild);
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  var notificationMessage = $(IDS.NOTIFICATION_MESSAGE);
  notificationMessage.textContent =
      configData.translatedStrings.thumbnailRemovedNotification;
  var undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, KEYCODE.ENTER, onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;
  var restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, KEYCODE.ENTER, onUndo);
  restoreAllLink.textContent =
      configData.translatedStrings.restoreThumbnailsShort;
  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  var notificationCloseButton = $(IDS.NOTIFICATION_CLOSE_BUTTON);
  notificationCloseButton.addEventListener('click', hideNotification);

  userInitiatedMostVisitedChange = false;
  window.addEventListener('resize', onResize);
  onResize();

  var topLevelHandle = getEmbeddedSearchApiHandle();

  ntpApiHandle = topLevelHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  ntpApiHandle.oninputstart = onInputStart;
  ntpApiHandle.oninputcancel = restoreNtp;

  if (ntpApiHandle.isInputInProgress)
    onInputStart();

  onThemeChange();
  onMostVisitedChange();

  searchboxApiHandle = topLevelHandle.searchBox;

  if (fakebox) {
    // Listener for updating the key capture state.
    document.body.onmousedown = function(event) {
      if (isFakeboxClick(event))
        searchboxApiHandle.startCapturingKeyStrokes();
      else if (isFakeboxFocused())
        searchboxApiHandle.stopCapturingKeyStrokes();
    };
    searchboxApiHandle.onkeycapturechange = function() {
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
    };
    var inputbox = $(IDS.FAKEBOX_INPUT);
    if (inputbox) {
      inputbox.onpaste = function(event) {
        event.preventDefault();
        searchboxApiHandle.paste();
      };
      inputbox.ondrop = function(event) {
        event.preventDefault();
        var text = event.dataTransfer.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
      };
      inputbox.ondragenter = function() {
        setFakeboxDragFocus(true);
      };
      inputbox.ondragleave = function() {
        setFakeboxDragFocus(false);
      };
    }

    // Update the fakebox style to match the current key capturing state.
    setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
    // Add class for setting alignments based on language directionality.
    document.body.classList.add(CLASSES.RTL);
    $(IDS.TILES).dir = 'rtl';
  }
}


/**
 * Binds event listeners.
 */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}

return {
  init: init,
  listen: listen
};
}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
