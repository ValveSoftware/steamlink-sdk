// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

  // Flag values for ctrl, alt and shift as defined by EventFlags
  // in "event_constants.h".
  // @enum {number}
  var Modifier = {
    NONE: 0,
    ALT: 8,
    CONTROL: 4,
    SHIFT: 2
  }

  // Each virtual key event is assigned a unique ID.
  var nextRequestID = 0;

  // Keycodes have been deprecated in the KeyEvent specification, but are
  // nonetheless required to support legacy web content.  The Keycodes in the
  // following table are based on subset of US-EN 101-key keyboard. These
  // codes are used in the absence of explicit keycodes for kb-key and
  // kb-keysequence elements. Keyboard layout authors may explicitly set the
  // keyCode attribute for kb-key or kb-keysequence elements to refer to
  // indices in this table in order to emulate a physical keyboard with an
  // alternate layout.  Not all keys on a virtual keyboard are required to
  // have keyCodes. The shiftModifier specifies whether to always include or
  // exclude the shift modifer when sending key events for this key. If it's
  // undefined, it will defer to state of the keyboard.
  // TODO(rsadam): Correctly propagate shutdown keycode. This is currently
  // ignored due to chromoting (crbug/146609)
  var keyCodes = {
    '\b': {keyCode: 0x08, keyName: 'Backspace', shiftModifier: false},
    '\t': {keyCode: 0x09, keyName: 'Tab', shiftModifier: false},
    '\n': {keyCode: 0x0D, keyName: 'Enter', shiftModifier: false},
    'Esc': {keyCode: 0x1B, keyName: 'Escape', shiftModifier: false},
    ' ': {keyCode: 0x20, keyName: 'Space', shiftModifier: false},
    'Arrow-Left': {keyCode: 0x25, keyName: 'ArrowLeft',
        shiftModifier: undefined},
    'Arrow-Up': {keyCode: 0x26, keyName: 'ArrowUp', shiftModifier: undefined},
    'Arrow-Right': {keyCode: 0x27, keyName: 'ArrowRight',
        shiftModifier: undefined},
    'Arrow-Down': {keyCode: 0x28, keyName: 'ArrowDown',
        shiftModifier: undefined},
    '0': {keyCode: 0x30, keyName: 'Digit0', shiftModifier: false},
    ')': {keyCode: 0x30, keyName: 'Digit0', shiftModifier: true},
    '1': {keyCode: 0x31, keyName: 'Digit1', shiftModifier: false},
    '!': {keyCode: 0x31, keyName: 'Digit1', shiftModifier: true},
    '2': {keyCode: 0x32, keyName: 'Digit2', shiftModifier: false},
    '@': {keyCode: 0x32, keyName: 'Digit2', shiftModifier: true},
    '3': {keyCode: 0x33, keyName: 'Digit3', shiftModifier: false},
    '#': {keyCode: 0x33, keyName: 'Digit3', shiftModifier: true},
    '4': {keyCode: 0x34, keyName: 'Digit4', shiftModifier: false},
    '$': {keyCode: 0x34, keyName: 'Digit4', shiftModifier: true},
    '5': {keyCode: 0x35, keyName: 'Digit5', shiftModifier: false},
    '%': {keyCode: 0x35, keyName: 'Digit5', shiftModifier: true},
    '6': {keyCode: 0x36, keyName: 'Digit6', shiftModifier: false},
    '^': {keyCode: 0x36, keyName: 'Digit6', shiftModifier: true},
    '7': {keyCode: 0x37, keyName: 'Digit7', shiftModifier: false},
    '&': {keyCode: 0x37, keyName: 'Digit7', shiftModifier: true},
    '8': {keyCode: 0x38, keyName: 'Digit8', shiftModifier: false},
    '*': {keyCode: 0x38, keyName: 'Digit8', shiftModifier: true},
    '9': {keyCode: 0x39, keyName: 'Digit9', shiftModifier: false},
    '(': {keyCode: 0x39, keyName: 'Digit9', shiftModifier: true},
    'a': {keyCode: 0x41, keyName: 'KeyA', shiftModifier: false},
    'A': {keyCode: 0x41, keyName: 'KeyA', shiftModifier: true},
    'b': {keyCode: 0x42, keyName: 'KeyB', shiftModifier: false},
    'B': {keyCode: 0x42, keyName: 'KeyB', shiftModifier: true},
    'c': {keyCode: 0x43, keyName: 'KeyC', shiftModifier: false},
    'C': {keyCode: 0x43, keyName: 'KeyC', shiftModifier: true},
    'd': {keyCode: 0x44, keyName: 'KeyD', shiftModifier: false},
    'D': {keyCode: 0x44, keyName: 'KeyD', shiftModifier: true},
    'e': {keyCode: 0x45, keyName: 'KeyE', shiftModifier: false},
    'E': {keyCode: 0x45, keyName: 'KeyE', shiftModifier: true},
    'f': {keyCode: 0x46, keyName: 'KeyF', shiftModifier: false},
    'F': {keyCode: 0x46, keyName: 'KeyF', shiftModifier: true},
    'g': {keyCode: 0x47, keyName: 'KeyG', shiftModifier: false},
    'G': {keyCode: 0x47, keyName: 'KeyG', shiftModifier: true},
    'h': {keyCode: 0x48, keyName: 'KeyH', shiftModifier: false},
    'H': {keyCode: 0x48, keyName: 'KeyH', shiftModifier: true},
    'i': {keyCode: 0x49, keyName: 'KeyI', shiftModifier: false},
    'I': {keyCode: 0x49, keyName: 'KeyI', shiftModifier: true},
    'j': {keyCode: 0x4A, keyName: 'KeyJ', shiftModifier: false},
    'J': {keyCode: 0x4A, keyName: 'KeyJ', shiftModifier: true},
    'k': {keyCode: 0x4B, keyName: 'KeyK', shiftModifier: false},
    'K': {keyCode: 0x4B, keyName: 'KeyK', shiftModifier: true},
    'l': {keyCode: 0x4C, keyName: 'KeyL', shiftModifier: false},
    'L': {keyCode: 0x4C, keyName: 'KeyL', shiftModifier: true},
    'm': {keyCode: 0x4D, keyName: 'KeyM', shiftModifier: false},
    'M': {keyCode: 0x4D, keyName: 'KeyM', shiftModifier: true},
    'n': {keyCode: 0x4E, keyName: 'KeyN', shiftModifier: false},
    'N': {keyCode: 0x4E, keyName: 'KeyN', shiftModifier: true},
    'o': {keyCode: 0x4F, keyName: 'KeyO', shiftModifier: false},
    'O': {keyCode: 0x4F, keyName: 'KeyO', shiftModifier: true},
    'p': {keyCode: 0x50, keyName: 'KeyP', shiftModifier: false},
    'P': {keyCode: 0x50, keyName: 'KeyP', shiftModifier: true},
    'q': {keyCode: 0x51, keyName: 'KeyQ', shiftModifier: false},
    'Q': {keyCode: 0x51, keyName: 'KeyQ', shiftModifier: true},
    'r': {keyCode: 0x52, keyName: 'KeyR', shiftModifier: false},
    'R': {keyCode: 0x52, keyName: 'KeyR', shiftModifier: true},
    's': {keyCode: 0x53, keyName: 'KeyS', shiftModifier: false},
    'S': {keyCode: 0x53, keyName: 'KeyS', shiftModifier: true},
    't': {keyCode: 0x54, keyName: 'KeyT', shiftModifier: false},
    'T': {keyCode: 0x54, keyName: 'KeyT', shiftModifier: true},
    'u': {keyCode: 0x55, keyName: 'KeyU', shiftModifier: false},
    'U': {keyCode: 0x55, keyName: 'KeyU', shiftModifier: true},
    'v': {keyCode: 0x56, keyName: 'KeyV', shiftModifier: false},
    'V': {keyCode: 0x56, keyName: 'KeyV', shiftModifier: true},
    'w': {keyCode: 0x57, keyName: 'KeyW', shiftModifier: false},
    'W': {keyCode: 0x57, keyName: 'KeyW', shiftModifier: true},
    'x': {keyCode: 0x58, keyName: 'KeyX', shiftModifier: false},
    'X': {keyCode: 0x58, keyName: 'KeyX', shiftModifier: true},
    'y': {keyCode: 0x59, keyName: 'KeyY', shiftModifier: false},
    'Y': {keyCode: 0x59, keyName: 'KeyY', shiftModifier: true},
    'z': {keyCode: 0x5A, keyName: 'KeyZ', shiftModifier: false},
    'Z': {keyCode: 0x5A, keyName: 'KeyZ', shiftModifier: true},
    'Fullscreen': {keyCode: 0x7A, shiftModifier: false},
    'Shutdown': {keyCode: 0x98, shiftModifier: false},
    'Back': {keyCode: 0xA6, shiftModifier: false},
    'Forward': {keyCode: 0xA7, shiftModifier: false},
    'Reload': {keyCode: 0xA8, shiftModifier: false},
    'Search': {keyCode: 0xAA, shiftModifier: false},
    'Mute': {keyCode: 0xAD, keyName: 'VolumeMute', shiftModifier: false},
    'Volume-Down': {keyCode: 0xAE, keyName: 'VolumeDown',
        shiftModifier: false},
    'Volume-Up': {keyCode: 0xAF, keyName: 'VolumeUp', shiftModifier: false},
    'Change-Window': {keyCode: 0xB6, shiftModifier: false},
    ';': {keyCode: 0xBA, keyName: 'Semicolon', shiftModifier: false},
    ':': {keyCode: 0xBA, keyName: 'Semicolon',shiftModifier: true},
    '=': {keyCode: 0xBB, keyName: 'Equal', shiftModifier: false},
    '+': {keyCode: 0xBB, keyName: 'Equal', shiftModifier: true},
    ',': {keyCode: 0xBC, keyName: 'Comma', shiftModifier: false},
    '<': {keyCode: 0xBC, keyName: 'Comma', shiftModifier: true},
    '-': {keyCode: 0xBD, keyName: 'Minus', shiftModifier: false},
    '_': {keyCode: 0xBD, keyName: 'Minus', shiftModifier: true},
    '.': {keyCode: 0xBE, keyName: 'Period', shiftModifier: false},
    '>': {keyCode: 0xBE, keyName: 'Period', shiftModifier: true},
    '/': {keyCode: 0xBF, keyName: 'Slash', shiftModifier: false},
    '?': {keyCode: 0xBF, keyName: 'Slash', shiftModifier: true},
    '`': {keyCode: 0xC0, keyName: 'Backquote', shiftModifier: false},
    '~': {keyCode: 0xC0, keyName: 'Backquote', shiftModifier: true},
    'Brightness-Down': {keyCode: 0xD8, keyName: 'BrightnessDown',
        shiftModifier: false},
    'Brightness-Up': {keyCode: 0xD9, keyName: 'BrightnessUp',
        shiftModifier: false},
    '[': {keyCode: 0xDB, keyName: 'BracketLeft', shiftModifier: false},
    '{': {keyCode: 0xDB, keyName: 'BracketLeft', shiftModifier: true},
    '\\': {keyCode: 0xDC, keyName: 'Backslash', shiftModifier: false},
    '|': {keyCode: 0xDC, keyName: 'Backslash', shiftModifier: true},
    ']': {keyCode: 0xDD, keyName: 'BracketRight', shiftModifier: false},
    '}': {keyCode: 0xDD, keyName: 'BracketRight', shiftModifier: true},
    '\'': {keyCode: 0xDE, keyName: 'Quote', shiftModifier: false},
    '"': {keyCode: 0xDE, keyName: 'Quote', shiftModifier: true},
  };

  Polymer('kb-key-codes', {
    /**
     * Retrieves the keyCode and status of the shift modifier.
     * @param {string} id ID of an entry in the code table.
     * @return {keyCode: numeric, shiftModifier: boolean}
     */
    GetKeyCodeAndModifiers: function(id) {
      var entry = keyCodes[id];
      if (entry) {
        return {
          keyCode: entry.keyCode,
          keyName: entry.keyName || 'Unidentified',
          shiftModifier: entry.shiftModifier
        };
      }
      if (id.length != 1)
        return;
      // Special case of accented characters.
      return {
        keyCode: 0,
        keyName: 'Unidentified',
        shiftModifier: false
      };
    },

   /**
    * Creates a virtual key event for use with the keyboard extension API.
    * See http://www.w3.org/TR/DOM-Level-3-Events/#events-KeyboardEvent.
    * @param {Object} detail Attribute of the key being pressed or released.
    * @param {string} type The type of key event, which may be keydown
    *     or keyup.
    * @return {?KeyboardEvent} A KeyboardEvent object, or undefined on
    *     failure.
    */
   createVirtualKeyEvent: function(detail, type) {
     var char = detail.char;
     var keyCode = detail.keyCode;
     var keyName = detail.keyName;
     // The shift modifier is handled specially. Some charactares like '+'
     // {keyCode: 0xBB, shiftModifier: true}, are available on non-upper
     // keysets, and so we rely on caching the correct shiftModifier. If
     // the cached value of the shiftModifier is undefined, we defer to
     // the shiftModifier in the detail.
     var shiftModifier = detail.shiftModifier;
     if (keyCode == undefined || keyName == undefined) {
       var state = this.GetKeyCodeAndModifiers(char);
       if (state) {
         keyCode = keyCode || state.keyCode;
         keyName = keyName || state.keyName;
         shiftModifier = (state.shiftModifier == undefined) ?
             shiftModifier : state.shiftModifier;
       } else {
         // Keycode not defined.
         return;
       }
     }
     var modifiers = Modifier.NONE;
     modifiers = shiftModifier ? modifiers | Modifier.SHIFT : modifiers;
     modifiers = detail.controlModifier ?
         modifiers | Modifier.CONTROL : modifiers;
     modifiers = detail.altModifier ? modifiers | Modifier.ALT : modifiers;
     return {
       type: type,
       charValue: char.charCodeAt(0),
       keyCode: keyCode,
       keyName: keyName,
       modifiers: modifiers
     };
   },
  });
})();
