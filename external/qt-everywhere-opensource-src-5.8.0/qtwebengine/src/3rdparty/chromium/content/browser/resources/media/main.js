// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A global object that gets used by the C++ interface.
 */
var media = (function() {
  'use strict';

  var manager = null;

  // A number->string mapping that is populated through the backend that
  // describes the phase that the network entity is in.
  var eventPhases = {};

  // A number->string mapping that is populated through the backend that
  // describes the type of event sent from the network.
  var eventTypes = {};

  // A mapping of number->CacheEntry where the number is a unique id for that
  // network request.
  var cacheEntries = {};

  // A mapping of url->CacheEntity where the url is the url of the resource.
  var cacheEntriesByKey = {};

  var requrestURLs = {};

  var media = {
    BAR_WIDTH: 200,
    BAR_HEIGHT: 25
  };

  /**
   * Users of |media| must call initialize prior to calling other methods.
   */
  media.initialize = function(theManager) {
    manager = theManager;
  };

  media.onReceiveAudioStreamData = function(audioStreamData) {
    for (var component in audioStreamData) {
      media.updateAudioComponent(audioStreamData[component]);
    }
  };

  media.onReceiveVideoCaptureCapabilities = function(videoCaptureCapabilities) {
    manager.updateVideoCaptureCapabilities(videoCaptureCapabilities)
  }

  media.onReceiveConstants = function(constants) {
    for (var key in constants.eventTypes) {
      var value = constants.eventTypes[key];
      eventTypes[value] = key;
    }

    for (var key in constants.eventPhases) {
      var value = constants.eventPhases[key];
      eventPhases[value] = key;
    }
  };

  media.cacheForUrl = function(url) {
    return cacheEntriesByKey[url];
  };

  media.onNetUpdate = function(updates) {
    updates.forEach(function(update) {
      var id = update.source.id;
      if (!cacheEntries[id])
        cacheEntries[id] = new media.CacheEntry;

      switch (eventPhases[update.phase] + '.' + eventTypes[update.type]) {
        case 'PHASE_BEGIN.DISK_CACHE_ENTRY_IMPL':
          var key = update.params.key;

          // Merge this source with anything we already know about this key.
          if (cacheEntriesByKey[key]) {
            cacheEntriesByKey[key].merge(cacheEntries[id]);
            cacheEntries[id] = cacheEntriesByKey[key];
          } else {
            cacheEntriesByKey[key] = cacheEntries[id];
          }
          cacheEntriesByKey[key].key = key;
          break;

        case 'PHASE_BEGIN.SPARSE_READ':
          cacheEntries[id].readBytes(update.params.offset,
                                      update.params.buff_len);
          cacheEntries[id].sparse = true;
          break;

        case 'PHASE_BEGIN.SPARSE_WRITE':
          cacheEntries[id].writeBytes(update.params.offset,
                                       update.params.buff_len);
          cacheEntries[id].sparse = true;
          break;

        case 'PHASE_BEGIN.URL_REQUEST_START_JOB':
          requrestURLs[update.source.id] = update.params.url;
          break;

        case 'PHASE_NONE.HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
          // Record the total size of the file if this was a range request.
          var range = /content-range:\s*bytes\s*\d+-\d+\/(\d+)/i.exec(
              update.params.headers);
          var key = requrestURLs[update.source.id];
          delete requrestURLs[update.source.id];
          if (range && key) {
            if (!cacheEntriesByKey[key]) {
              cacheEntriesByKey[key] = new media.CacheEntry;
              cacheEntriesByKey[key].key = key;
            }
            cacheEntriesByKey[key].size = range[1];
          }
          break;
      }
    });
  };

  media.onRendererTerminated = function(renderId) {
    util.object.forEach(manager.players_, function(playerInfo, id) {
      if (playerInfo.properties['render_id'] == renderId) {
        manager.removePlayer(id);
      }
    });
  };

  media.updateAudioComponent = function(component) {
    var uniqueComponentId = component.owner_id + ':' + component.component_id;
    switch (component.status) {
      case 'closed':
        manager.removeAudioComponent(
            component.component_type, uniqueComponentId);
        break;
      default:
        manager.updateAudioComponent(
            component.component_type, uniqueComponentId, component);
        break;
    }
  };

  media.onPlayerOpen = function(id, timestamp) {
    manager.addPlayer(id, timestamp);
  };

  media.onMediaEvent = function(event) {
    var source = event.renderer + ':' + event.player;

    // Although this gets called on every event, there is nothing we can do
    // because there is no onOpen event.
    media.onPlayerOpen(source);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'render_id', event.renderer);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'player_id', event.player);

    var propertyCount = 0;
    util.object.forEach(event.params, function(value, key) {
      key = key.trim();

      // These keys get spammed *a lot*, so put them on the display
      // but don't log list.
      if (key === 'buffer_start' ||
          key === 'buffer_end' ||
          key === 'buffer_current' ||
          key === 'is_downloading_data') {
        manager.updatePlayerInfoNoRecord(
            source, event.ticksMillis, key, value);
      } else {
        manager.updatePlayerInfo(source, event.ticksMillis, key, value);
      }
      propertyCount += 1;
    });

    if (propertyCount === 0) {
      manager.updatePlayerInfo(
          source, event.ticksMillis, 'event', event.type);
    }
  };

  // |chrome| is not defined during tests.
  if (window.chrome && window.chrome.send) {
    chrome.send('getEverything');
  }
  return media;
}());
