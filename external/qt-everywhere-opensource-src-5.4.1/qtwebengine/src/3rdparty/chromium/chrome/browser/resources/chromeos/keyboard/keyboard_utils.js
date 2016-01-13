// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for keyboard utility functions.
 */
var keyboard = {};

/**
 * Swallows keypress and keyup events of arrow keys.
 * @param {KeyboardEvent} event Raised event.
 * @private
 */
keyboard.onKeyIgnore_ = function(event) {
  if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey)
    return;

  if (event.keyIdentifier == 'Left' ||
      event.keyIdentifier == 'Right' ||
      event.keyIdentifier == 'Up' ||
      event.keyIdentifier == 'Down') {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * Converts arrow keys into tab/shift-tab key events.
 * @param {KeyboardEvent} event Raised event.
 * @private
 */
keyboard.onKeyDown_ = function(event) {
   if (event.ctrlKey || event.shiftKey || event.altKey || event.metaKey)
     return;

   var needsUpDownKeys = event.target.classList.contains('needs-up-down-keys');

   if (event.keyIdentifier == 'Left' ||
       (!needsUpDownKeys && event.keyIdentifier == 'Up')) {
     keyboard.raiseKeyFocusPrevious(document.activeElement);
     event.stopPropagation();
     event.preventDefault();
   } else if (event.keyIdentifier == 'Right' ||
              (!needsUpDownKeys && event.keyIdentifier == 'Down')) {
     keyboard.raiseKeyFocusNext(document.activeElement);
     event.stopPropagation();
     event.preventDefault();
   }
};

/**
 * Raises tab/shift-tab keyboard events.
 * @param {HTMLElement} element Element that should receive the event.
 * @param {string} eventType Keyboard event type.
 * @param {boolean} shift True if shift should be on.
 * @private
 */
keyboard.raiseTabKeyEvent_ = function(element, eventType, shift) {
  var event = document.createEvent('KeyboardEvent');
  event.initKeyboardEvent(
      eventType,
      true,  // canBubble
      true,  // cancelable
      window,
      'U+0009',
      0,  // keyLocation
      false,  // ctrl
      false,  // alt
      shift,  // shift
      false);  // meta
  element.dispatchEvent(event);
};

/**
 * Raises shift+tab keyboard events to focus previous element.
 * @param {HTMLElement} element Element that should receive the event.
 */
keyboard.raiseKeyFocusPrevious = function(element) {
  keyboard.raiseTabKeyEvent_(element, 'keydown', true);
  keyboard.raiseTabKeyEvent_(element, 'keypress', true);
  keyboard.raiseTabKeyEvent_(element, 'keyup', true);
};

/**
 * Raises tab keyboard events to focus next element.
 * @param {HTMLElement} element Element that should receive the event.
 */
keyboard.raiseKeyFocusNext = function(element) {
  keyboard.raiseTabKeyEvent_(element, 'keydown', false);
  keyboard.raiseTabKeyEvent_(element, 'keypress', false);
  keyboard.raiseTabKeyEvent_(element, 'keyup', false);
};

/**
 * Initializes event handling for arrow keys driven focus flow.
 */
keyboard.initializeKeyboardFlow = function() {
  document.addEventListener('keydown',
      keyboard.onKeyDown_, true);
  document.addEventListener('keypress',
      keyboard.onKeyIgnore_, true);
  document.addEventListener('keyup',
      keyboard.onKeyIgnore_, true);
};
