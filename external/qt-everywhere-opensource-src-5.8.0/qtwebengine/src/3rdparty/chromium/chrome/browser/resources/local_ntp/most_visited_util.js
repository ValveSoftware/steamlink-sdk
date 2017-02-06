// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Utilities for rendering most visited thumbnails and titles.
 */

<include src="instant_iframe_validation.js">


/**
 * The different types of events that are logged from the NTP.  This enum is
 * used to transfer information from the NTP javascript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
var NTP_LOGGING_EVENT_TYPE = {
  // The suggestion is coming from the server.
  NTP_SERVER_SIDE_SUGGESTION: 0,
  // The suggestion is coming from the client.
  NTP_CLIENT_SIDE_SUGGESTION: 1,
  // Indicates a tile was rendered, no matter if it's a thumbnail, a gray tile
  // or an external tile.
  NTP_TILE: 2,
  // The tile uses a local thumbnail image.
  NTP_THUMBNAIL_TILE: 3,
  // Used when no thumbnail is specified and a gray tile with the domain is used
  // as the main tile.
  NTP_GRAY_TILE: 4,
  // The visuals of that tile are handled externally by the page itself.
  NTP_EXTERNAL_TILE: 5,
  // There was an error in loading both the thumbnail image and the fallback
  // (if it was provided), resulting in a grey tile.
  NTP_THUMBNAIL_ERROR: 6,
  // Used a gray tile with the domain as the fallback for a failed thumbnail.
  NTP_GRAY_TILE_FALLBACK: 7,
  // The visuals of that tile's fallback are handled externally.
  NTP_EXTERNAL_TILE_FALLBACK: 8,
  // The user moused over an NTP tile or title.
  NTP_MOUSEOVER: 9,
  // A NTP Tile has finished loading (successfully or failing).
  NTP_TILE_LOADED: 10,
};

/**
 * Type of the impression provider for a generic client-provided suggestion.
 * @type {string}
 * @const
 */
var CLIENT_PROVIDER_NAME = 'client';

/**
 * Type of the impression provider for a generic server-provided suggestion.
 * @type {string}
 * @const
 */
var SERVER_PROVIDER_NAME = 'server';

/**
 * The origin of this request.
 * @const {string}
 */
var DOMAIN_ORIGIN = '{{ORIGIN}}';

/**
 * Parses query parameters from Location.
 * @param {string} location The URL to generate the CSS url for.
 * @return {Object} Dictionary containing name value pairs for URL.
 */
function parseQueryParams(location) {
  var params = Object.create(null);
  var query = location.search.substring(1);
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    var k = decodeURIComponent(pair[0]);
    if (k in params) {
      // Duplicate parameters are not allowed to prevent attackers who can
      // append things to |location| from getting their parameter values to
      // override legitimate ones.
      return Object.create(null);
    } else {
      params[k] = decodeURIComponent(pair[1]);
    }
  }
  return params;
}


/**
 * Creates a new most visited link element.
 * @param {Object} params URL parameters containing styles for the link.
 * @param {string} href The destination for the link.
 * @param {string} title The title for the link.
 * @param {string|undefined} text The text for the link or none.
 * @param {string|undefined} direction The text direction.
 * @param {string|undefined} provider A provider name (max 8 alphanumeric
 *     characters) used for logging. Undefined if suggestion is not coming from
 *     the server.
 * @return {HTMLAnchorElement} A new link element.
 */
function createMostVisitedLink(params, href, title, text, direction, provider) {
  var styles = getMostVisitedStyles(params, !!text);
  var link = document.createElement('a');
  link.style.color = styles.color;
  link.style.fontSize = styles.fontSize + 'px';
  if (styles.fontFamily)
    link.style.fontFamily = styles.fontFamily;
  if (styles.textAlign)
    link.style.textAlign = styles.textAlign;
  if (styles.textFadePos) {
    var dir = /^rtl$/i.test(direction) ? 'to left' : 'to right';
    // The fading length in pixels is passed by the caller.
    var mask = 'linear-gradient(' + dir + ', rgba(0,0,0,1), rgba(0,0,0,1) ' +
        styles.textFadePos + 'px, rgba(0,0,0,0))';
    link.style.textOverflow = 'clip';
    link.style.webkitMask = mask;
  }
  if (styles.numTitleLines && styles.numTitleLines > 1) {
    link.classList.add('multiline');
  }

  link.href = href;
  link.title = title;
  link.target = '_top';
  // Include links in the tab order.  The tabIndex is necessary for
  // accessibility.
  link.tabIndex = '0';
  if (text) {
    // Wrap text with span so ellipsis will appear at the end of multiline.
    var spanWrap = document.createElement('span');
    spanWrap.textContent = text;
    link.appendChild(spanWrap);
  }
  link.addEventListener('mouseover', function() {
    var ntpApiHandle = chrome.embeddedSearch.newTabPage;
    ntpApiHandle.logEvent(NTP_LOGGING_EVENT_TYPE.NTP_MOUSEOVER);
  });
  link.addEventListener('focus', function() {
    window.parent.postMessage('linkFocused', DOMAIN_ORIGIN);
  });
  link.addEventListener('blur', function() {
    window.parent.postMessage('linkBlurred', DOMAIN_ORIGIN);
  });

  var navigateFunction = function handleNavigation(e) {
    var isServerSuggestion = 'url' in params;

    // Ping are only populated for server-side suggestions, never for MV.
    if (isServerSuggestion && params.ping) {
      generatePing(DOMAIN_ORIGIN + params.ping);
    }

    var ntpApiHandle = chrome.embeddedSearch.newTabPage;
    if ('pos' in params && isFinite(params.pos)) {
      ntpApiHandle.logMostVisitedNavigation(parseInt(params.pos, 10),
                                            provider || '');
    }

    // Follow <a> normally, so transition type will be LINK.
  };

  link.addEventListener('click', navigateFunction);
  link.addEventListener('keydown', function(event) {
    if (event.keyCode == 46 /* DELETE */ ||
        event.keyCode == 8 /* BACKSPACE */) {
      event.preventDefault();
      window.parent.postMessage('tileBlacklisted,' + params.pos, DOMAIN_ORIGIN);
    } else if (event.keyCode == 13 /* ENTER */ ||
               event.keyCode == 32 /* SPACE */) {
      // Event target is the <a> tag. Send a click event on it, which will
      // trigger the 'click' event registered above.
      event.preventDefault();
      event.target.click();
    }
  });

  return link;
}


/**
 * Returns the color to display string with, depending on whether title is
 * displayed, the current theme, and URL parameters.
 * @param {Object<string>} params URL parameters specifying style.
 * @param {boolean} isTitle if the style is for the Most Visited Title.
 * @return {string} The color to use, in "rgba(#,#,#,#)" format.
 */
function getTextColor(params, isTitle) {
  // 'RRGGBBAA' color format overrides everything.
  if ('c' in params && params.c.match(/^[0-9A-Fa-f]{8}$/)) {
    // Extract the 4 pairs of hex digits, map to number, then form rgba().
    var t = params.c.match(/(..)(..)(..)(..)/).slice(1).map(function(s) {
      return parseInt(s, 16);
    });
    return 'rgba(' + t[0] + ',' + t[1] + ',' + t[2] + ',' + t[3] / 255 + ')';
  }

  // For backward compatibility with server-side NTP, look at themes directly
  // and use param.c for non-title or as fallback.
  var apiHandle = chrome.embeddedSearch.newTabPage;
  var themeInfo = apiHandle.themeBackgroundInfo;
  var c = '#777';
  if (isTitle && themeInfo && !themeInfo.usingDefaultTheme) {
    // Read from theme directly
    c = convertArrayToRGBAColor(themeInfo.textColorRgba) || c;
  } else if ('c' in params) {
    c = convertToHexColor(parseInt(params.c, 16)) || c;
  }
  return c;
}


/**
 * Decodes most visited styles from URL parameters.
 * - c: A hexadecimal number interpreted as a hex color code.
 * - f: font-family.
 * - fs: font-size as a number in pixels.
 * - ta: text-align property, as a string.
 * - tf: text fade starting position, in pixels.
 * - ntl: number of lines in the title.
 * @param {Object<string>} params URL parameters specifying style.
 * @param {boolean} isTitle if the style is for the Most Visited Title.
 * @return {Object} Styles suitable for CSS interpolation.
 */
function getMostVisitedStyles(params, isTitle) {
  var styles = {
    color: getTextColor(params, isTitle),  // Handles 'c' in params.
    fontFamily: '',
    fontSize: 11
  };
  if ('f' in params && /^[-0-9a-zA-Z ,]+$/.test(params.f))
    styles.fontFamily = params.f;
  if ('fs' in params && isFinite(parseInt(params.fs, 10)))
    styles.fontSize = parseInt(params.fs, 10);
  if ('ta' in params && /^[-0-9a-zA-Z ,]+$/.test(params.ta))
    styles.textAlign = params.ta;
  if ('tf' in params) {
    var tf = parseInt(params.tf, 10);
    if (isFinite(tf))
      styles.textFadePos = tf;
  }
  if ('ntl' in params) {
    var ntl = parseInt(params.ntl, 10);
    if (isFinite(ntl))
      styles.numTitleLines = ntl;
  }
  return styles;
}


/**
 * @param {string} location A location containing URL parameters.
 * @param {function(Object, Object)} fill A function called with styles and
 *     data to fill.
 */
function fillMostVisited(location, fill) {
  var params = parseQueryParams(location);
  params.rid = parseInt(params.rid, 10);
  if (!isFinite(params.rid) && !params.url)
    return;
  // Log whether the suggestion was obtained from the server or the client.
  chrome.embeddedSearch.newTabPage.logEvent(params.url ?
      NTP_LOGGING_EVENT_TYPE.NTP_SERVER_SIDE_SUGGESTION :
      NTP_LOGGING_EVENT_TYPE.NTP_CLIENT_SIDE_SUGGESTION);
  var data;
  if (params.url) {
    // Means that the suggestion data comes from the server. Create data object.
    data = {
      url: params.url,
      largeIconUrl: params.liu || '',
      thumbnailUrl: params.tu || '',
      title: params.ti || '',
      direction: params.di || '',
      domain: params.dom || '',
      provider: params.pr || SERVER_PROVIDER_NAME
    };
  } else {
    var apiHandle = chrome.embeddedSearch.searchBox;
    data = apiHandle.getMostVisitedItemData(params.rid);
    if (!data)
      return;
    // Allow server-side provider override.
    data.provider = params.pr || CLIENT_PROVIDER_NAME;
  }

  if (isFinite(params.dummy) && parseInt(params.dummy, 10)) {
    data.dummy = true;
  }
  if (/^javascript:/i.test(data.url) ||
      /^javascript:/i.test(data.thumbnailUrl) ||
      !/^[a-z0-9]{0,8}$/i.test(data.provider))
    return;
  if (data.direction)
    document.body.dir = data.direction;
  fill(params, data);
}


/**
 * Sends a POST request to ping url.
 * @param {string} url URL to be pinged.
 */
function generatePing(url) {
  if (navigator.sendBeacon) {
    navigator.sendBeacon(url);
  } else {
    // if sendBeacon is not enabled, we fallback for "a ping".
    var a = document.createElement('a');
    a.href = '#';
    a.ping = url;
    a.click();
  }
}
