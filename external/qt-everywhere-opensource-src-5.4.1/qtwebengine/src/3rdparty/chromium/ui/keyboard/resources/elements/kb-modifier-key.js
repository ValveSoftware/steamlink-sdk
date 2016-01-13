// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {

  /**
   * The possible states of the key.
   * @const
   * @type {Enum}
   */
  var KEY_STATES = {
    PRESSED: "pressed", // Key-down.
    UNLOCKED: "unlocked", // Default state.
    TAPPED: "tapped", // Key-down followed by key-up.
    CHORDING: "chording", // Chording mode.
  };

  /**
   * A map of the state of all modifier keys.
   * @type {Object}
   */
  var states = {};

  Polymer('kb-modifier-key', {
    up: function(event) {
      if (this.state == KEY_STATES.PRESSED)
        this.state = KEY_STATES.TAPPED;
      else
        this.state = KEY_STATES.UNLOCKED;
      this.super([event]);
    },

    down: function(event) {
      // First transition state so that populateDetails generates
      // correct data.
      switch (this.state) {
        case KEY_STATES.UNLOCKED:
          this.state = KEY_STATES.PRESSED;
          break;
        case KEY_STATES.TAPPED:
          this.state = KEY_STATES.UNLOCKED;
          break;
        case KEY_STATES.PRESSED:
        case KEY_STATES.CHORDING:
          // We pressed another key at the same time,
          // so ignore second press.
          return;
        default:
          console.error("Undefined key state: " + state);
          break;
      }
      this.super([event]);
    },

    /**
     * Returns whether the modifier for this key is active.
     * @return {boolean}
     */
    isActive: function() {
      return this.state != KEY_STATES.UNLOCKED;
    },

    /**
     * Notifies key that a non-control keyed down.
     * A control key is defined as one of shift, control or alt.
     */
    onNonControlKeyDown: function() {
      switch(this.state) {
        case (KEY_STATES.PRESSED):
          this.state = KEY_STATES.CHORDING;
          break;
      }
    },

    /**
     * Notifies key that a non-control keyed was typed.
     * A control key is defined as one of shift, control or alt.
     */
    onNonControlKeyTyped: function() {
       switch(this.state) {
         case (KEY_STATES.TAPPED):
           this.state = KEY_STATES.UNLOCKED;
           break;
       }
    },

    /**
     * Called on a pointer-out event. Ends chording.
     * @param {event} event The pointer-out event.
     */
    out: function(event) {
      // TODO(rsadam): Add chording event so that we don't reset
      // when shift-chording.
      if (this.state == KEY_STATES.CHORDING) {
        this.state = KEY_STATES.UNLOCKED;
      }
    },

    /*
     * Overrides the autoRelease function to enable chording.
     */
    autoRelease: function() {
    },

    populateDetails: function(caller) {
      var detail = this.super([caller]);
      if (this.state != KEY_STATES.UNLOCKED)
        detail.activeModifier = this.charValue;
      return detail;
    },

    /**
     *  Resets the modifier key state.
     */
    reset: function() {
      this.state = KEY_STATES.UNLOCKED;
    },

    get state() {
      var key = this.charValue;
      if (!key)
        console.error("missing key for kb-modifier-key state: " + this);
      // All keys default to the unlock state.
      if (!(key in states))
        states[key] = KEY_STATES.UNLOCKED;
      return states[key];
    },

    set state(value) {
      var key = this.charValue;
      states[key] = value;
    }
  });
})();
