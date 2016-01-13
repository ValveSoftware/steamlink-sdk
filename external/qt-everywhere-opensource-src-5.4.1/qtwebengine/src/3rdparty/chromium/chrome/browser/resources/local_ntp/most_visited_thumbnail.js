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
          params, data.url, data.title, undefined, data.provider);
      var domain = document.createElement('div');
      domain.textContent = data.domain;
      link.appendChild(domain);
      displayLink(link);
    }
    // Called on intentionally empty tiles for which the visuals are handled
    // externally by the page itself.
    function showEmptyTile() {
      displayLink(createMostVisitedLink(
          params, data.url, data.title, undefined, data.provider));
    }
    // Creates and adds an image.
    function createThumbnail(src) {
      var image = document.createElement('img');
      image.onload = function() {
        var shadow = document.createElement('span');
        shadow.className = 'shadow';
        var link = createMostVisitedLink(
            params, data.url, data.title, undefined, data.provider);
        link.appendChild(shadow);
        link.appendChild(image);
        displayLink(link);
      };
      image.onerror = function() {
        if (data.domain) {
          showDomainElement();
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_GRAY_TILE_FALLBACK);
        } else {
          showEmptyTile();
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_EXTERNAL_TILE_FALLBACK);
        }
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ERROR);
      };
      image.src = src;
    }

    if (data.thumbnailUrl) {
      createThumbnail(data.thumbnailUrl);
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
