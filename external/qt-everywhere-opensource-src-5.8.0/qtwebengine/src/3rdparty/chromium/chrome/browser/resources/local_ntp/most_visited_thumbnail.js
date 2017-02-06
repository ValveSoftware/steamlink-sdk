// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Rendering for iframed most visited thumbnails.
 */

window.addEventListener('DOMContentLoaded', function() {
  'use strict';

  fillMostVisited(document.location, function(params, data) {
    function logEvent(eventName) {
      chrome.embeddedSearch.newTabPage.logEvent(eventName);
    }
    function logMostVisitedImpression(tileIndex, provider) {
      chrome.embeddedSearch.newTabPage.logMostVisitedImpression(
          tileIndex, provider);
    }
    function displayLink(link) {
      document.body.appendChild(link);
      window.parent.postMessage('linkDisplayed', '{{ORIGIN}}');
    }
    function showDomainElement() {
      var link = createMostVisitedLink(
          params, data.url, data.title, undefined, data.direction,
          data.provider);
      var domain = document.createElement('div');
      domain.textContent = data.domain;
      link.appendChild(domain);
      displayLink(link);
    }
    // Called on intentionally empty tiles for which the visuals are handled
    // externally by the page itself.
    function showEmptyTile() {
      displayLink(createMostVisitedLink(
          params, data.url, data.title, undefined, data.direction,
          data.provider));
    }
    // Creates and adds an image.
    function createThumbnail(src, imageClass) {
      var image = document.createElement('img');
      if (imageClass) {
        image.classList.add(imageClass);
      }
      image.onload = function() {
        var link = createMostVisitedLink(
            params, data.url, data.title, undefined, data.direction,
            data.provider);
        // Use blocker to prevent context menu from showing image-related items.
        var blocker = document.createElement('span');
        blocker.className = 'blocker';
        link.appendChild(blocker);
        link.appendChild(image);
        displayLink(link);
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_TILE_LOADED);
      };
      image.onerror = function() {
        // If no external thumbnail fallback (etfb), and have domain.
        if (!params.etfb && data.domain) {
          showDomainElement();
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_GRAY_TILE_FALLBACK);
        } else {
          showEmptyTile();
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_EXTERNAL_TILE_FALLBACK);
        }
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ERROR);
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_TILE_LOADED);
      };
      image.src = src;
    }

    var useIcons = params['icons'] == '1';
    if (data.dummy) {
      showEmptyTile();
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_EXTERNAL_TILE);
    } else if (useIcons && data.largeIconUrl) {
      createThumbnail(data.largeIconUrl, 'large-icon');
      // TODO(huangs): Log event for large icons.
    } else if (!useIcons && data.thumbnailUrls && data.thumbnailUrls.length) {
      createThumbnail(data.thumbnailUrls[0], 'thumbnail');
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_TILE);
    } else if (data.domain) {
      showDomainElement();
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_GRAY_TILE);
    } else {
      showEmptyTile();
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_EXTERNAL_TILE);
    }
    logEvent(NTP_LOGGING_EVENT_TYPE.NTP_TILE);

    // Log an impression if we know the position of the tile.
    if (isFinite(params.pos) && data.provider) {
      logMostVisitedImpression(parseInt(params.pos, 10), data.provider);
    }
  });
});
