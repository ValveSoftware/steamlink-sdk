// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The long-press delay in milliseconds before long-press handler is
 * invoked.
 * @const
 * @type {number}
 */
var LONGPRESS_DELAY_MSEC = 500;

/**
 * The maximum number of elements in one keyset rule.
 * @const
 * @type {number}
 */
var MAXIMUM_NUM_OF_RULE_ELEMENTS = 3;

/**
 * The minumum number of elements in one keyset rule.
 * @const
 * @type {number}
 */
var MINIMUM_NUM_OF_RULE_ELEMENTS = 2;

/**
 * The index of event type element in the splitted keyset rule.
 * @const
 * @type {number}
 */
var EVENT_TYPE = 0;

/**
 * The index of toKeyset element in the splitted keyset rule.
 * @const
 * @type {number}
 */
var TO_KEYSET = 1;

/**
 * The index of nextKeyset element in the splitted keyset rule.
 * @const
 * @type {number}
 */
var NEXT_KEYSET = 2;

/**
 * The index offset of toKeyset and nextKeyset elements in splitted keyset
 * rule array and the array in keysetRules.
 * @const
 * @type {number}
 */
var OFFSET = 1;

/**
 * The minumum number of elements in one keyset rule.
 * @const {number}
 */
var MINIMUM_NUM_OF_RULE_ELEMENTS = 2;

Polymer('kb-key-base', {
  repeat: false,
  invert: false,
  longPressTimer: null,
  pointerId: undefined,

  /**
   * The keyset transition rules. It defines which keyset to transit to on
   * which key events. It consists at most four rules (down, up, long, dbl).
   * If no rule is defined for a key event, the event wont trigger a keyset
   * change.
   * @type {Object.<string, Array.<string>}
   */
  keysetRules: null,

  ready: function() {
    if (this.toKeyset) {
      // Parsing keyset rules from toKeyset attribute string.
      // An rule can be defined as: (down|up|long|dbl):keysetid[:keysetid]
      // and each rule are separated by a semicolon. The first element
      // defines the event this rule applies to. The second element defines
      // what keyset to transit to after event received. The third optional
      // element defines what keyset to transit to after a key is pressed in
      // the new keyset. It is useful when you want to transit to a not
      // locked keyset. For example, after transit to a upper case keyset,
      // it may make sense to transit back to lower case after user typed
      // any key at the upper case keyset.
      var rules =
          this.toKeyset.replace(/(\r\n|\n|\r| )/g, '').split(';');
      this.keysetRules = {};
      var self = this;
      rules.forEach(function(element) {
        if (element == '')
          return;
        var keyValues = element.split(':', MAXIMUM_NUM_OF_RULE_ELEMENTS);
        if (keyValues.length < MINIMUM_NUM_OF_RULE_ELEMENTS) {
          console.error('Invalid keyset rules: ' + element);
          return;
        }
        self.keysetRules[keyValues[EVENT_TYPE]] = [keyValues[TO_KEYSET],
            (keyValues[NEXT_KEYSET] ? keyValues[NEXT_KEYSET] : null)];
      });
    }
  },

  down: function(event) {
    this.pointerId = event.pointerId;
    var detail = this.populateDetails('down');
    this.fire('key-down', detail);
    this.longPressTimer = this.generateLongPressTimer();
  },
  out: function(event) {
    this.classList.remove('active');
    clearTimeout(this.longPressTimer);
  },
  up: function(event) {
    this.generateKeyup();
  },

  /**
   * Releases the pressed key programmatically and fires key-up event. This
   * should be called if a second key is pressed while the first key is not
   * released yet.
   */
  autoRelease: function() {
    this.generateKeyup();
  },

  /**
   * Drops the pressed key.
   */
  dropKey: function() {
    this.classList.remove('active');
    clearTimeout(this.longPressTimer);
    this.pointerId = undefined;
  },

  /**
   * Populates details for this key, and then fires a key-up event.
   */
  generateKeyup: function() {
    if (this.pointerId === undefined)
      return;

    // Invalidates the pointerId so the subsequent pointerup event is a
    // noop.
    this.pointerId = undefined;
    clearTimeout(this.longPressTimer);
    var detail = this.populateDetails('up');
    this.fire('key-up', detail);
  },

  /**
   * Character value associated with the key. Typically, the value is a
   * single character, but may be multi-character in cases like a ".com"
   * button.
   * @return {string}
   */
  get charValue() {
    return this.invert ? this.hintText : (this.char || this.textContent);
  },

  /**
   * Hint text value associated with the key. Typically, the value is a
   * single character.
   * @return {string}
   */
  get hintTextValue() {
    return this.invert ? (this.char || this.textContent) : this.hintText;
  },

  /**
   * Handles a swipe flick that originated from this key.
   * @param {detail} The details of the swipe.
   */
  onFlick: function(detail) {
    if (!(detail.direction & SwipeDirection.UP) || !this.hintTextValue)
      return;
    var typeDetails = {char: this.hintTextValue};
    this.fire('type-key', typeDetails);
  },

  /**
   * Returns a subset of the key attributes.
   * @param {string} caller The id of the function which called
   *     populateDetails.
   */
  populateDetails: function(caller) {
    var detail = {
      char: this.charValue,
      toLayout: this.toLayout,
      repeat: this.repeat
    };

    switch (caller) {
      case ('up'):
        if (this.keysetRules && this.keysetRules.up != undefined) {
          detail.toKeyset = this.keysetRules.up[TO_KEYSET - OFFSET];
          detail.nextKeyset = this.keysetRules.up[NEXT_KEYSET - OFFSET];
        }
        break;
      case ('down'):
        if (this.keysetRules && this.keysetRules.down != undefined) {
          detail.toKeyset = this.keysetRules.down[TO_KEYSET - OFFSET];
          detail.nextKeyset = this.keysetRules.down[NEXT_KEYSET - OFFSET];
        }
        break;
      default:
        break;
    }
    return detail;
  },

  generateLongPressTimer: function() {
    return this.async(function() {
      var detail = {
        char: this.charValue,
        hintText: this.hintTextValue
      };
      if (this.keysetRules && this.keysetRules.long != undefined) {
        detail.toKeyset = this.keysetRules.long[TO_KEYSET - OFFSET];
        detail.nextKeyset = this.keysetRules.long[NEXT_KEYSET - OFFSET];
      }
      this.fire('key-longpress', detail);
    }, null, LONGPRESS_DELAY_MSEC);
  },
});
