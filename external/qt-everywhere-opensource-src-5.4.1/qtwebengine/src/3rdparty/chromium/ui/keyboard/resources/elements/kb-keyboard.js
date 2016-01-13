// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The repeat delay in milliseconds before a key starts repeating. Use the
 * same rate as Chromebook.
 * (See chrome/browser/chromeos/language_preferences.cc)
 * @const
 * @type {number}
 */
var REPEAT_DELAY_MSEC = 500;

/**
 * The repeat interval or number of milliseconds between subsequent
 * keypresses. Use the same rate as Chromebook.
 * @const
 * @type {number}
 */
var REPEAT_INTERVAL_MSEC = 50;

/**
 * The double click/tap interval.
 * @const
 * @type {number}
 */
var DBL_INTERVAL_MSEC = 300;

/**
 * The index of the name of the keyset when searching for all keysets.
 * @const
 * @type {number}
 */
var REGEX_KEYSET_INDEX = 1;

/**
 * The integer number of matches when searching for keysets.
 * @const
 * @type {number}
 */
var REGEX_MATCH_COUNT = 2;

/**
 * The boolean to decide if keyboard should transit to upper case keyset
 * when spacebar is pressed. If a closing punctuation is followed by a
 * spacebar, keyboard should automatically transit to upper case.
 * @type {boolean}
 */
var enterUpperOnSpace = false;

/**
 * A structure to track the currently repeating key on the keyboard.
 */
var repeatKey = {

  /**
    * The timer for the delay before repeating behaviour begins.
    * @type {number|undefined}
    */
  timer: undefined,

  /**
   * The interval timer for issuing keypresses of a repeating key.
   * @type {number|undefined}
   */
  interval: undefined,

  /**
   * The key which is currently repeating.
   * @type {BaseKey|undefined}
   */
  key: undefined,

  /**
   * Cancel the repeat timers of the currently active key.
   */
  cancel: function() {
    clearTimeout(this.timer);
    clearInterval(this.interval);
    this.timer = undefined;
    this.interval = undefined;
    this.key = undefined;
  }
};

/**
 * The minimum movement interval needed to trigger cursor move on
 * horizontal and vertical way.
 * @const
 * @type {number}
 */
var MIN_SWIPE_DIST_X = 50;
var MIN_SWIPE_DIST_Y = 20;

/**
 * The maximum swipe distance that will trigger hintText of a key
 * to be typed.
 * @const
 * @type {number}
 */
var MAX_SWIPE_FLICK_DIST = 60;

/**
 * The boolean to decide if it is swipe in process or finished.
 * @type {boolean}
 */
var swipeInProgress = false;

// Flag values for ctrl, alt and shift as defined by EventFlags
// in "event_constants.h".
// @enum {number}
var Modifier = {
  NONE: 0,
  ALT: 8,
  CONTROL: 4,
  SHIFT: 2
};

/**
 * A structure to track the current swipe status.
 */
var swipeTracker = {
  /**
   * The latest PointerMove event in the swipe.
   * @type {Object}
   */
  currentEvent: undefined,

  /**
   * Whether or not a swipe changes direction.
   * @type {false}
   */
  isComplex: false,

  /**
   * The count of horizontal and vertical movement.
   * @type {number}
   */
  offset_x : 0,
  offset_y : 0,

  /**
   * Last touch coordinate.
   * @type {number}
   */
  pre_x : 0,
  pre_y : 0,

  /**
   * The PointerMove event which triggered the swipe.
   * @type {Object}
   */
  startEvent: undefined,

  /**
   * The flag of current modifier key.
   * @type {number}
   */
  swipeFlags : 0,

  /**
   * Current swipe direction.
   * @type {number}
   */
  swipeDirection : 0,

  /**
   * The number of times we've swiped within a single swipe.
   * @type {number}
   */
  swipeIndex: 0,

  /**
   * Returns the combined direction of the x and y offsets.
   * @return {number} The latest direction.
   */
  getOffsetDirection: function() {
    // TODO (rsadam): Use angles to figure out the direction.
    var direction = 0;
    // Checks for horizontal swipe.
    if (Math.abs(this.offset_x) > MIN_SWIPE_DIST_X) {
      if (this.offset_x > 0) {
        direction |= SwipeDirection.RIGHT;
      } else {
        direction |= SwipeDirection.LEFT;
      }
    }
    // Checks for vertical swipe.
    if (Math.abs(this.offset_y) > MIN_SWIPE_DIST_Y) {
      if (this.offset_y < 0) {
        direction |= SwipeDirection.UP;
      } else {
        direction |= SwipeDirection.DOWN;
      }
    }
    return direction;
  },

  /**
   * Populates the swipe update details.
   * @param {boolean} endSwipe Whether this is the final event for this
   *     swipe.
   * @return {Object} The current state of the swipeTracker.
   */
  populateDetails: function(endSwipe) {
    var detail = {};
    detail.direction = this.swipeDirection;
    detail.index = this.swipeIndex;
    detail.status = this.swipeStatus;
    detail.endSwipe = endSwipe;
    detail.startEvent = this.startEvent;
    detail.currentEvent = this.currentEvent;
    detail.isComplex = this.isComplex;
    return detail;
  },

  /**
   * Reset all the values when swipe finished.
   */
  resetAll: function() {
    this.offset_x = 0;
    this.offset_y = 0;
    this.pre_x = 0;
    this.pre_y = 0;
    this.swipeFlags = 0;
    this.swipeDirection = 0;
    this.swipeIndex = 0;
    this.startEvent = undefined;
    this.currentEvent = undefined;
    this.isComplex = false;
  },

  /**
   * Updates the swipe path with the current event.
   * @param {Object} event The PointerEvent that triggered this update.
   * @return {boolean} Whether or not to notify swipe observers.
   */
  update: function(event) {
    if(!event.isPrimary)
      return false;
    // Update priors.
    this.offset_x += event.screenX - this.pre_x;
    this.offset_y += event.screenY - this.pre_y;
    this.pre_x = event.screenX;
    this.pre_y = event.screenY;

    // Check if movement crosses minimum thresholds in each direction.
    var direction = this.getOffsetDirection();
    if (direction == 0)
      return false;
    // If swipeIndex is zero the current event is triggering the swipe.
    if (this.swipeIndex == 0) {
      this.startEvent = event;
    } else if (direction != this.swipeDirection) {
      // Toggle the isComplex flag.
      this.isComplex = true;
    }
    // Update the swipe tracker.
    this.swipeDirection = direction;
    this.offset_x = 0;
    this.offset_y = 0;
    this.currentEvent = event;
    this.swipeIndex++;
    return true;
  },

};

Polymer('kb-keyboard', {
  alt: null,
  config: null,
  control: null,
  dblDetail_: null,
  dblTimer_: null,
  inputType: null,
  lastPressedKey: null,
  shift: null,
  sounds: {},
  stale: true,
  swipeHandler: null,
  voiceInput_: null,
  //TODO(rsadam@): Add a control to let users change this.
  volume: DEFAULT_VOLUME,

  /**
   * The default input type to keyboard layout map. The key must be one of
   * the input box type values.
   * @type {object}
   */
  inputTypeToLayoutMap: {
    number: "numeric",
    text: "qwerty",
    password: "qwerty"
  },

  /**
   * Caches the specified sound on the keyboard.
   * @param {string} soundId The name of the .wav file in the "sounds"
       directory.
   */
  addSound: function(soundId) {
    // Check if already loaded.
    if (soundId == Sound.NONE || this.sounds[soundId])
      return;
    var pool = [];
    for (var i = 0; i < SOUND_POOL_SIZE; i++) {
      var audio = document.createElement('audio');
      audio.preload = "auto";
      audio.id = soundId;
      audio.src = "../sounds/" + soundId + ".wav";
      audio.volume = this.volume;
      pool.push(audio);
    }
    this.sounds[soundId] = pool;
  },

  /**
   * Changes the current keyset.
   * @param {Object} detail The detail of the event that called this
   *     function.
   */
  changeKeyset: function(detail) {
    if (detail.relegateToShift && this.shift) {
      this.keyset = this.shift.textKeyset;
      this.activeKeyset.nextKeyset = undefined;
      return true;
    }
    var toKeyset = detail.toKeyset;
    if (toKeyset) {
      this.keyset = toKeyset;
      this.activeKeyset.nextKeyset = detail.nextKeyset;
      return true;
    }
    return false;
  },

  keysetChanged: function() {
    var keyset = this.activeKeyset;
    // Show the keyset if it has been initialized.
    if (keyset)
      keyset.show();
  },

  configChanged: function() {
    this.layout = this.config.layout;
  },

  ready: function() {
    this.voiceInput_ = new VoiceInput(this);
    this.swipeHandler = this.move.bind(this);
    var self = this;
    getKeyboardConfig(function(config) {
      self.config = config;
    });
  },

  /**
   * Registers a callback for state change events.
   * @param{!Function} callback Callback function to register.
   */
  addKeysetChangedObserver: function(callback) {
    this.addEventListener('stateChange', callback);
  },

  /**
   * Called when the type of focused input box changes. If a keyboard layout
   * is defined for the current input type, that layout will be loaded.
   * Otherwise, the keyboard layout for 'text' type will be loaded.
   */
  inputTypeChanged: function() {
    // Disable layout switching at accessbility mode.
    if (this.config && this.config.a11ymode)
      return;

    // TODO(bshe): Toggle visibility of some keys in a keyboard layout
    // according to the input type.
    var layout = this.inputTypeToLayoutMap[this.inputType];
    if (!layout)
      layout = this.inputTypeToLayoutMap.text;
    this.layout = layout;
  },

  /**
   * When double click/tap event is enabled, the second key-down and key-up
   * events on the same key should be skipped. Return true when the event
   * with |detail| should be skipped.
   * @param {Object} detail The detail of key-up or key-down event.
   */
  skipEvent: function(detail) {
    if (this.dblDetail_) {
      if (this.dblDetail_.char != detail.char) {
        // The second key down is not on the same key. Double click/tap
        // should be ignored.
        this.dblDetail_ = null;
        clearTimeout(this.dblTimer_);
      } else if (this.dblDetail_.clickCount == 1) {
        return true;
      }
    }
    return false;
  },

  /**
   * Handles a swipe update.
   * param {Object} detail The swipe update details.
   */
  onSwipeUpdate: function(detail) {
    var direction = detail.direction;
    if (!direction)
      console.error("Swipe direction cannot be: " + direction);
    // Triggers swipe editting if it's a purely horizontal swipe.
    if (!(direction & (SwipeDirection.UP | SwipeDirection.DOWN))) {
      // Nothing to do if the swipe has ended.
      if (detail.endSwipe)
        return;
      var modifiers = 0;
      // TODO (rsadam): This doesn't take into account index shifts caused
      // by vertical swipes.
      if (detail.index % 2 != 0) {
        modifiers |= Modifier.SHIFT;
        modifiers |= Modifier.CONTROL;
      }
      MoveCursor(direction, modifiers);
      return;
    }
    // Triggers swipe hintText if it's a purely vertical swipe.
    if (this.activeKeyset.flick &&
        !(direction & (SwipeDirection.LEFT | SwipeDirection.RIGHT))) {
      // Check if event is relevant to us.
      if ((!detail.endSwipe) || (detail.isComplex))
        return;
      // Too long a swipe.
      var distance = Math.abs(detail.startEvent.screenY -
          detail.currentEvent.screenY);
      if (distance > MAX_SWIPE_FLICK_DIST)
        return;
      var triggerKey = detail.startEvent.target;
      if (triggerKey && triggerKey.onFlick)
        triggerKey.onFlick(detail);
    }
  },

  /**
   * This function is bound to swipeHandler. Updates the current swipe
   * status so that PointerEvents can be converted to Swipe events.
   * @param {PointerEvent} event.
   */
  move: function(event) {
    if (!swipeTracker.update(event))
      return;
    // Conversion was successful, swipe is now in progress.
    swipeInProgress = true;
    if (this.lastPressedKey) {
      this.lastPressedKey.classList.remove('active');
      this.lastPressedKey = null;
    }
    this.onSwipeUpdate(swipeTracker.populateDetails(false));
  },

  /**
   * Handles key-down event that is sent by kb-key-base.
   * @param {CustomEvent} event The key-down event dispatched by
   *     kb-key-base.
   * @param {Object} detail The detail of pressed kb-key.
   */
  keyDown: function(event, detail) {
    if (this.skipEvent(detail))
      return;

    if (this.lastPressedKey) {
      this.lastPressedKey.classList.remove('active');
      this.lastPressedKey.autoRelease();
    }
    this.lastPressedKey = event.target;
    this.lastPressedKey.classList.add('active');
    repeatKey.cancel();
    this.playSound(detail.sound);

    var char = detail.char;
    switch(char) {
      case 'Shift':
        this.classList.remove('caps-locked');
        break;
      case 'Alt':
      case 'Ctrl':
        var modifier = char.toLowerCase() + "-active";
        // Removes modifier if already active.
        if (this.classList.contains(modifier))
          this.classList.remove(modifier);
        break;
      case 'Invalid':
        // Not all Invalid keys are transition keys. Reset control keys if
        // we pressed a transition key.
        if (event.target.toKeyset || detail.relegateToShift)
          this.onNonControlKeyTyped();
        break;
      default:
        // Notify shift key.
        if (this.shift)
          this.shift.onNonControlKeyDown();
        if (this.ctrl)
          this.ctrl.onNonControlKeyDown();
        if (this.alt)
          this.alt.onNonControlKeyDown();
        break;
    }
    if(this.changeKeyset(detail))
      return;
    if (detail.repeat) {
      this.keyTyped(detail);
      this.onNonControlKeyTyped();
      repeatKey.key = this.lastPressedKey;
      var self = this;
      repeatKey.timer = setTimeout(function() {
        repeatKey.timer = undefined;
        repeatKey.interval = setInterval(function() {
           self.playSound(detail.sound);
           self.keyTyped(detail);
        }, REPEAT_INTERVAL_MSEC);
      }, Math.max(0, REPEAT_DELAY_MSEC - REPEAT_INTERVAL_MSEC));
    }
  },

  /**
   * Handles key-out event that is sent by kb-shift-key.
   * @param {CustomEvent} event The key-out event dispatched by
   *     kb-shift-key.
   * @param {Object} detail The detail of pressed kb-shift-key.
   */
  keyOut: function(event, detail) {
    this.changeKeyset(detail);
  },

  /**
   * Enable/start double click/tap event recognition.
   * @param {CustomEvent} event The enable-dbl event dispatched by
   *     kb-shift-key.
   * @param {Object} detail The detail of pressed kb-shift-key.
   */
  enableDbl: function(event, detail) {
    if (!this.dblDetail_) {
      this.dblDetail_ = detail;
      this.dblDetail_.clickCount = 0;
      var self = this;
      this.dblTimer_ = setTimeout(function() {
        self.dblDetail_.callback = null;
        self.dblDetail_ = null;
      }, DBL_INTERVAL_MSEC);
    }
  },

  /**
   * Enable the selection while swipe.
   * @param {CustomEvent} event The enable-dbl event dispatched by
   *    kb-shift-key.
   */
  enableSel: function(event) {
    // TODO(rsadam): Disabled for now. May come back if we revert swipe
    // selection to not do word selection.
  },

  /**
   * Handles pointerdown event. This is used for swipe selection process.
   * to get the start pre_x and pre_y. And also add a pointermove handler
   * to start handling the swipe selection event.
   * @param {PointerEvent} event The pointerup event that received by
   *     kb-keyboard.
   */
  down: function(event) {
    var layout = getKeysetLayout(this.activeKeysetId);
    var key = layout.findClosestKey(event.clientX, event.clientY);
    if (key)
      key.down(event);
    if (event.isPrimary) {
      swipeTracker.pre_x = event.screenX;
      swipeTracker.pre_y = event.screenY;
      this.addEventListener("pointermove", this.swipeHandler, false);
    }
  },

  /**
   * Handles pointerup event. This is used for double tap/click events.
   * @param {PointerEvent} event The pointerup event that bubbled to
   *     kb-keyboard.
   */
  up: function(event) {
    var layout = getKeysetLayout(this.activeKeysetId);
    var key = layout.findClosestKey(event.clientX, event.clientY);
    if (key)
      key.up(event);
    // When touch typing, it is very possible that finger moves slightly out
    // of the key area before releases. The key should not be dropped in
    // this case.
    // TODO(rsadam@) Change behaviour such that the key drops and the second
    // key gets pressed.
    if (this.lastPressedKey &&
        this.lastPressedKey.pointerId == event.pointerId) {
      this.lastPressedKey.autoRelease();
    }

    if (this.dblDetail_) {
      this.dblDetail_.clickCount++;
      if (this.dblDetail_.clickCount == 2) {
        this.dblDetail_.callback();
        this.changeKeyset(this.dblDetail_);
        clearTimeout(this.dblTimer_);

        this.classList.add('caps-locked');

        this.dblDetail_ = null;
      }
    }

    // TODO(zyaozhujun): There are some edge cases to deal with later.
    // (for instance, what if a second finger trigger a down and up
    // event sequence while swiping).
    // When pointer up from the screen, a swipe selection session finished,
    // all the data should be reset to prepare for the next session.
    if (event.isPrimary && swipeInProgress) {
      swipeInProgress = false;
      this.onSwipeUpdate(swipeTracker.populateDetails(true))
      swipeTracker.resetAll();
    }
    this.removeEventListener('pointermove', this.swipeHandler, false);
  },

  /**
   * Handles PointerOut event. This is used for when a swipe gesture goes
   * outside of the keyboard window.
   * @param {Object} event The pointerout event that bubbled to the
   *    kb-keyboard.
   */
  out: function(event) {
    repeatKey.cancel();
    // Ignore if triggered from one of the keys.
    if (this.compareDocumentPosition(event.relatedTarget) &
        Node.DOCUMENT_POSITION_CONTAINED_BY)
      return;
    if (swipeInProgress)
      this.onSwipeUpdate(swipeTracker.populateDetails(true))
    // Touched outside of the keyboard area, so disables swipe.
    swipeInProgress = false;
    swipeTracker.resetAll();
    this.removeEventListener('pointermove', this.swipeHandler, false);
  },

  /**
   * Handles a TypeKey event. This is used for when we programmatically
   * want to type a specific key.
   * @param {CustomEvent} event The TypeKey event that bubbled to the
   *    kb-keyboard.
   */
  type: function(event) {
    this.keyTyped(event.detail);
  },

  /**
   * Handles key-up event that is sent by kb-key-base.
   * @param {CustomEvent} event The key-up event dispatched by kb-key-base.
   * @param {Object} detail The detail of pressed kb-key.
   */
  keyUp: function(event, detail) {
    if (this.skipEvent(detail))
      return;
    if (swipeInProgress)
      return;
    if (detail.activeModifier) {
      var modifier = detail.activeModifier.toLowerCase() + "-active";
      this.classList.add(modifier);
    }
    // Adds the current keyboard modifiers to the detail.
    if (this.ctrl)
      detail.controlModifier = this.ctrl.isActive();
    if (this.alt)
      detail.altModifier = this.alt.isActive();
    if (this.lastPressedKey)
      this.lastPressedKey.classList.remove('active');
    // Keyset transition key. This is needed to transition from upper
    // to lower case when we are not in caps mode, as well as when
    // we're ending chording.
    this.changeKeyset(detail);

    if (this.lastPressedKey &&
        this.lastPressedKey.charValue != event.target.charValue) {
      return;
    }
    if (repeatKey.key == event.target) {
      repeatKey.cancel();
      this.lastPressedKey = null;
      return;
    }
    var toLayoutId = detail.toLayout;
    // Layout transition key.
    if (toLayoutId)
      this.layout = toLayoutId;
    var char = detail.char;
    this.lastPressedKey = null;
    // Characters that should not be typed.
    switch(char) {
      case 'Invalid':
      case 'Shift':
      case 'Ctrl':
      case 'Alt':
        enterUpperOnSpace = false;
        swipeTracker.swipeFlags = 0;
        return;
      case 'Microphone':
        this.voiceInput_.onDown();
        return;
      default:
        break;
    }
    // Tries to type the character. Resorts to insertText if that fails.
    if(!this.keyTyped(detail))
      insertText(char);
    // Post-typing logic.
    switch(char) {
      case '\n':
      case ' ':
        if(enterUpperOnSpace) {
          enterUpperOnSpace = false;
          if (this.shift) {
            var shiftDetail = this.shift.onSpaceAfterPunctuation();
            // Check if transition defined.
            this.changeKeyset(shiftDetail);
          } else {
            console.error('Capitalization on space after punctuation \
                        enabled, but cannot find target keyset.');
          }
          // Immediately return to maintain shift-state. Space is a
          // non-control key and would otherwise trigger a reset of the
          // shift key, causing a transition to lower case.
          return;
        }
        break;
      case '.':
      case '?':
      case '!':
        enterUpperOnSpace = this.shouldUpperOnSpace();
        break;
      default:
        enterUpperOnSpace = false;
        break;
    }
    // Reset control keys.
    this.onNonControlKeyTyped();
  },

  /*
   * Handles key-longpress event that is sent by kb-key-base.
   * @param {CustomEvent} event The key-longpress event dispatched by
   *     kb-key-base.
   * @param {Object} detail The detail of pressed key.
   */
  keyLongpress: function(event, detail) {
    // If the gesture is long press, remove the pointermove listener.
    this.removeEventListener('pointermove', this.swipeHandler, false);
    // Keyset transtion key.
    if (this.changeKeyset(detail)) {
      // Locks the keyset before removing active to prevent flicker.
      this.classList.add('caps-locked');
      // Makes last pressed key inactive if transit to a new keyset on long
      // press.
      if (this.lastPressedKey)
        this.lastPressedKey.classList.remove('active');
    }
  },

  /**
   * Plays the specified sound.
   * @param {Sound} sound The id of the audio tag.
   */
  playSound: function(sound) {
    if (!SOUND_ENABLED || !sound || sound == Sound.NONE)
      return;
    var pool = this.sounds[sound];
    if (!pool) {
      console.error("Cannot find audio tag: " + sound);
      return;
    }
    // Search the sound pool for a free resource.
    for (var i = 0; i < pool.length; i++) {
      if (pool[i].paused) {
        pool[i].play();
        return;
      }
    }
  },

  /**
   * Whether we should transit to upper case when seeing a space after
   * punctuation.
   * @return {boolean}
   */
  shouldUpperOnSpace: function() {
    // TODO(rsadam): Add other input types in which we should not
    // transition to upper after a space.
    return this.inputTypeValue != 'password';
  },

  /**
   * Handler for the 'set-layout' event.
   * @param {!Event} event The triggering event.
   * @param {{layout: string}} details Details of the event, which contains
   *     the name of the layout to activate.
   */
  setLayout: function(event, details) {
    this.layout = details.layout;
  },

  /**
   * Handles a change in the keyboard layout. Auto-selects the default
   * keyset for the new layout.
   */
  layoutChanged: function() {
    this.stale = true;
    if (!this.selectDefaultKeyset()) {
      console.error('No default keyset found for layout: ' + this.layout);
      return;
    }
    this.activeKeyset.show();
  },

  /**
   * Notifies the modifier keys that a non-control key was typed. This
   * lets them reset sticky behaviour. A non-control key is defined as
   * any key that is not Control, Alt, or Shift.
   */
  onNonControlKeyTyped: function() {
    if (this.shift)
      this.shift.onNonControlKeyTyped();
    if (this.ctrl)
      this.ctrl.onNonControlKeyTyped();
    if (this.alt)
      this.alt.onNonControlKeyTyped();
    this.classList.remove('ctrl-active');
    this.classList.remove('alt-active');
  },

  /**
   * Callback function for when volume is changed.
   */
  volumeChanged: function() {
    var toChange = Object.keys(this.sounds);
    for (var i = 0; i < toChange.length; i++) {
      var pool = this.sounds[toChange[i]];
      for (var j = 0; j < pool.length; j++) {
        pool[j].volume = this.volume;
      }
    }
  },

  /**
   * Id for the active keyset.
   * @type {string}
   */
  get activeKeysetId() {
    return this.layout + '-' + this.keyset;
  },

  /**
   * The active keyset DOM object.
   * @type {kb-keyset}
   */
  get activeKeyset() {
    return this.querySelector('#' + this.activeKeysetId);
  },

  /**
   * The current input type.
   * @type {string}
   */
  get inputTypeValue() {
    return this.inputType;
  },

  /**
   * Changes the input type if it's different from the current
   * type, else resets the keyset to the default keyset.
   * @type {string}
   */
  set inputTypeValue(value) {
    if (value == this.inputType)
      this.selectDefaultKeyset();
    else
      this.inputType = value;
  },

  /**
   * The keyboard is ready for input once the target keyset appears
   * in the distributed nodes for the keyboard.
   * @return {boolean} Indicates if the keyboard is ready for input.
   */
  isReady: function() {
    var keyset =  this.activeKeyset;
    if (!keyset)
      return false;
    var nodes = this.$.content.getDistributedNodes();
    for (var i = 0; i < nodes.length; i++) {
      if (nodes[i].id && nodes[i].id == keyset.id)
        return true;
    }
    return false;
  },

  /**
   * Generates fabricated key events to simulate typing on a
   * physical keyboard.
   * @param {Object} detail Attributes of the key being typed.
   * @return {boolean} Whether the key type succeeded.
   */
  keyTyped: function(detail) {
    var builder = this.$.keyCodeMetadata;
    if (this.ctrl)
      detail.controlModifier = this.ctrl.isActive();
    if (this.alt)
      detail.altModifier = this.alt.isActive();
    var downEvent = builder.createVirtualKeyEvent(detail, "keydown");
    if (downEvent) {
      sendKeyEvent(downEvent);
      sendKeyEvent(builder.createVirtualKeyEvent(detail, "keyup"));
      return true;
    }
    return false;
  },

  /**
   * Selects the default keyset for a layout.
   * @return {boolean} True if successful. This method can fail if the
   *     keysets corresponding to the layout have not been injected.
   */
  selectDefaultKeyset: function() {
    var keysets = this.querySelectorAll('kb-keyset');
    // Full name of the keyset is of the form 'layout-keyset'.
    var regex = new RegExp('^' + this.layout + '-(.+)');
    var keysetsLoaded = false;
    for (var i = 0; i < keysets.length; i++) {
      var matches = keysets[i].id.match(regex);
      if (matches && matches.length == REGEX_MATCH_COUNT) {
         keysetsLoaded = true;
         // Without both tests for a default keyset, it is possible to get
         // into a state where multiple layouts are displayed.  A
         // reproducable test case is do the following set of keyset
         // transitions: qwerty -> system -> dvorak -> qwerty.
         // TODO(kevers): Investigate why this is the case.
         if (keysets[i].isDefault ||
             keysets[i].getAttribute('isDefault') == 'true') {
           this.keyset = matches[REGEX_KEYSET_INDEX];
           this.classList.remove('caps-locked');
           this.classList.remove('alt-active');
           this.classList.remove('ctrl-active');
           // Caches shift key.
           this.shift = this.querySelector('kb-shift-key');
           if (this.shift)
             this.shift.reset();
           // Caches control key.
           this.ctrl = this.querySelector('kb-modifier-key[char=Ctrl]');
           if (this.ctrl)
             this.ctrl.reset();
           // Caches alt key.
           this.alt = this.querySelector('kb-modifier-key[char=Alt]');
           if (this.alt)
             this.alt.reset();
           this.fire('stateChange', {
             state: 'keysetLoaded',
             value: this.keyset,
           });
           keyboardLoaded();
           return true;
         }
      }
    }
    if (keysetsLoaded)
      console.error('No default keyset found for ' + this.layout);
    return false;
  }
});
