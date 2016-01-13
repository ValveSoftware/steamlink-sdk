// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
var Manager = (function() {
  'use strict';

  function Manager(clientRenderer) {
    this.players_ = {};
    this.audioComponents_ = [];
    this.clientRenderer_ = clientRenderer;
  }

  Manager.prototype = {
    /**
     * Updates an audio-component.
     * @param componentType Integer AudioComponent enum value; must match values
     * from the AudioLogFactory::AudioComponent enum.
     * @param componentId The unique-id of the audio-component.
     * @param componentData The actual component data dictionary.
     */
    updateAudioComponent: function(componentType, componentId, componentData) {
      if (!(componentType in this.audioComponents_))
        this.audioComponents_[componentType] = {};
      if (!(componentId in this.audioComponents_[componentType])) {
        this.audioComponents_[componentType][componentId] = componentData;
      } else {
        for (var key in componentData) {
          this.audioComponents_[componentType][componentId][key] =
              componentData[key];
        }
      }
      this.clientRenderer_.audioComponentAdded(
          componentType, this.audioComponents_[componentType]);
    },

    /**
     * Removes an audio-stream from the manager.
     * @param id The unique-id of the audio-stream.
     */
    removeAudioComponent: function(componentType, componentId) {
      if (!(componentType in this.audioComponents_) ||
          !(componentId in this.audioComponents_[componentType])) {
        return;
      }

      delete this.audioComponents_[componentType][componentId];
      this.clientRenderer_.audioComponentRemoved(
          componentType, this.audioComponents_[componentType]);
    },

    /**
     * Adds a player to the list of players to manage.
     */
    addPlayer: function(id) {
      if (this.players_[id]) {
        return;
      }
      // Make the PlayerProperty and add it to the mapping
      this.players_[id] = new PlayerInfo(id);
      this.clientRenderer_.playerAdded(this.players_, this.players_[id]);
    },

    /**
     * Attempts to remove a player from the UI.
     * @param id The ID of the player to remove.
     */
    removePlayer: function(id) {
      delete this.players_[id];
      this.clientRenderer_.playerRemoved(this.players_, this.players_[id]);
    },

    updatePlayerInfoNoRecord: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addPropertyNoRecord(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    },

    /**
     *
     * @param id The unique ID that identifies the player to be updated.
     * @param timestamp The timestamp of when the change occured.  This
     * timestamp is *not* normalized.
     * @param key The name of the property to be added/changed.
     * @param value The value of the property.
     */
    updatePlayerInfo: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addProperty(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    }
  };

  return Manager;
}());
