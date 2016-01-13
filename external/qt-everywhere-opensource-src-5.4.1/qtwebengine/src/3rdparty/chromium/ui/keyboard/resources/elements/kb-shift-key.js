// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {

  /**
   * The possible states of the shift key.
   * Unlocked is the default state. Locked for capslocked, pressed is a
   * key-down and tapped for a key-down followed by an immediate key-up.
   * @const
   * @type {Enum}
   */
  var KEY_STATES = {
    PRESSED: "pressed", // Key-down on shift key.
    LOCKED: "locked", // Key is capslocked.
    UNLOCKED: "unlocked", // Default state.
    TAPPED: "tapped", // Key-down followed by key-up.
    CHORDING: "chording" // Key-down followed by other keys.
  };

  /**
   * The pointerdown event on shiftkey that may eventually trigger chording
   * state. pointerId and eventTarget are the two fields that is used now.
   * @type {PointerEvent}
   */
  var enterChordingEvent = undefined;

  /**
   * Uses a closure to define one long press timer among all shift keys
   * regardless of the layout they are in.
   * @type {function}
   */
  var shiftLongPressTimer = undefined;

  /**
   * The current state of the shift key.
   * @type {Enum}
   */
  var state = KEY_STATES.UNLOCKED;

  Polymer('kb-shift-key', {
    /**
     * Defines how capslock effects keyset transition. We always transition
     * from the lowerCaseKeysetId to the upperCaseKeysetId if capslock is
     * on.
     * @type {string}
     */
    lowerCaseKeysetId: 'lower',
    upperCaseKeysetId: 'upper',

    up: function(event) {
      if (state == KEY_STATES.CHORDING &&
          event.pointerId != enterChordingEvent.pointerId) {
        // Disables all other pointer events on shift keys when chording.
        return;
      }
      switch (state) {
        case KEY_STATES.PRESSED:
          state = KEY_STATES.TAPPED;
          break;
        case KEY_STATES.CHORDING:
          // Leaves chording only if the pointer that triggered it is
          // released.
          state = KEY_STATES.UNLOCKED;
          break;
        default:
          break;
      }
      // When releasing the shift key, it is not the same shift key that was
      // pressed. Updates the pointerId of the releasing shift key to make
      // sure key-up event fires correctly in kb-key-base.
      this.pointerId = enterChordingEvent.pointerId;
      this.super([event]);
    },

    out: function(event) {
      // Sliding off the shift key while chording is treated as a key-up.
      // Note that we switch to a new keyset on shift keydown, and a finger
      // movement on the new shift key will trigger this function being
      // called on the old shift key. We should not end chording in that
      // case.
      if (state == KEY_STATES.CHORDING &&
          event.pointerId == enterChordingEvent.pointerId &&
          event.target != enterChordingEvent.target) {
        state = KEY_STATES.UNLOCKED;
        var detail = this.populateDetails('out');
        this.fire("key-out", detail);
      }
    },

    down: function(event) {
      // First transition state so that populateDetails generates
      // correct data.
      switch (state) {
        case KEY_STATES.UNLOCKED:
          state = KEY_STATES.PRESSED;
          break;
        case KEY_STATES.TAPPED:
        case KEY_STATES.LOCKED:
          state = KEY_STATES.UNLOCKED;
          break;
        case KEY_STATES.PRESSED:
        case KEY_STATES.CHORDING:
          // We pressed another shift key at the same time,
          // so ignore second press.
          return;
        default:
          console.error("Undefined shift key state: " + state);
          break;
      }
      enterChordingEvent = event;
      // Trigger parent behaviour.
      this.super([event]);
      this.fire('enable-sel');
      // Populate double click transition details.
      var detail = {};
      detail.char = this.char || this.textContent;
      detail.toKeyset = this.upperCaseKeysetId;
      detail.nextKeyset = undefined;
      detail.callback = this.onDoubleClick;
      this.fire('enable-dbl', detail);
    },

    generateLongPressTimer: function() {
      return this.async(function() {
        var detail = this.populateDetails();
        if (state == KEY_STATES.LOCKED) {
          // We don't care about the longpress if we are already
          // capitalized.
          return;
        } else {
          state = KEY_STATES.LOCKED;
          detail.toKeyset = this.upperCaseKeysetId;
          detail.nextKeyset = undefined;
        }
        this.fire('key-longpress', detail);
      }, null, LONGPRESS_DELAY_MSEC);
    },

    // @return Whether the shift modifier is currently active.
    isActive: function() {
      return state != KEY_STATES.UNLOCKED;
    },

    /**
     * Callback function for when a double click is triggered.
     */
    onDoubleClick: function() {
      state = KEY_STATES.LOCKED;
    },

    /**
     * Notifies shift key that a non-control key was pressed down.
     * A control key is defined as one of shift, control or alt.
     */
    onNonControlKeyDown: function() {
      switch (state) {
        case (KEY_STATES.PRESSED):
          state = KEY_STATES.CHORDING;
          // Disable longpress timer.
          clearTimeout(shiftLongPressTimer);
          break;
        default:
          break;
      }
    },

    /**
     * Notifies key that a non-control keyed was typed.
     * A control key is defined as one of shift, control or alt.
     */
    onNonControlKeyTyped: function() {
      if (state == KEY_STATES.TAPPED)
        state = KEY_STATES.UNLOCKED;
    },

    /**
     * Callback function for when a space is pressed after punctuation.
     * @return {Object} The keyset transitions the keyboard should make.
     */
    onSpaceAfterPunctuation: function() {
       var detail = {};
       detail.toKeyset = this.upperCaseKeysetId;
       detail.nextKeyset = this.lowerCaseKeysetId;
       state = KEY_STATES.TAPPED;
       return detail;
    },

    populateDetails: function(caller) {
      var detail = this.super([caller]);
      switch(state) {
        case(KEY_STATES.LOCKED):
          detail.toKeyset = this.upperCaseKeysetId;
          break;
        case(KEY_STATES.UNLOCKED):
          detail.toKeyset = this.lowerCaseKeysetId;
          break;
        case(KEY_STATES.PRESSED):
          detail.toKeyset = this.upperCaseKeysetId;
          break;
        case(KEY_STATES.TAPPED):
          detail.toKeyset = this.upperCaseKeysetId;
          detail.nextKeyset = this.lowerCaseKeysetId;
          break;
        case(KEY_STATES.CHORDING):
          detail.toKeyset = this.lowerCaseKeysetId;
          break;
        default:
          break;
      }
      return detail;
    },

    /**
     *  Resets the shift key state.
     */
    reset: function() {
      state = KEY_STATES.UNLOCKED;
    },

    /**
     * Overrides longPressTimer for the shift key.
     */
    get longPressTimer() {
      return shiftLongPressTimer;
    },

    set longPressTimer(timer) {
      shiftLongPressTimer = timer;
    },

    get state() {
      return state;
    },

    get textKeyset() {
      switch (state) {
        case KEY_STATES.UNLOCKED:
          return this.lowerCaseKeysetId;
        default:
          return this.upperCaseKeysetId;
      }
    },
  });
})();
