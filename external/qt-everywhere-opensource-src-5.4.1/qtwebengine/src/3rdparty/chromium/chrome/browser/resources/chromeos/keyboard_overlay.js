// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="keyboard_overlay_data.js"/>
<include src="keyboard_overlay_accessibility_helper.js"/>

var BASE_KEYBOARD = {
  top: 0,
  left: 0,
  width: 1237,
  height: 514
};

var BASE_INSTRUCTIONS = {
  top: 194,
  left: 370,
  width: 498,
  height: 142
};

var MODIFIER_TO_CLASS = {
  'SHIFT': 'modifier-shift',
  'CTRL': 'modifier-ctrl',
  'ALT': 'modifier-alt',
  'SEARCH': 'modifier-search'
};

var IDENTIFIER_TO_CLASS = {
  '2A': 'is-shift',
  '1D': 'is-ctrl',
  '38': 'is-alt',
  'E0 5B': 'is-search'
};

var LABEL_TO_IDENTIFIER = {
  'search': 'E0 5B',
  'ctrl': '1D',
  'alt': '38',
  'caps lock': '3A',
  'esc': '01',
  'disabled': 'DISABLED'
};

var KEYCODE_TO_LABEL = {
  8: 'backspace',
  9: 'tab',
  13: 'enter',
  27: 'esc',
  32: 'space',
  33: 'pageup',
  34: 'pagedown',
  35: 'end',
  36: 'home',
  37: 'left',
  38: 'up',
  39: 'right',
  40: 'down',
  46: 'delete',
  91: 'search',
  92: 'search',
  96: '0',
  97: '1',
  98: '2',
  99: '3',
  100: '4',
  101: '5',
  102: '6',
  103: '7',
  104: '8',
  105: '9',
  106: '*',
  107: '+',
  109: '-',
  110: '.',
  111: '/',
  112: 'back',
  113: 'forward',
  114: 'reload',
  115: 'full screen',
  116: 'switch window',
  117: 'bright down',
  118: 'bright up',
  119: 'mute',
  120: 'vol. down',
  121: 'vol. up',
  186: ';',
  187: '+',
  188: ',',
  189: '-',
  190: '.',
  191: '/',
  192: '`',
  219: '[',
  220: '\\',
  221: ']',
  222: '\'',
};

var IME_ID_PREFIX = '_comp_ime_';
var EXTENSION_ID_LEN = 32;

var keyboardOverlayId = 'en_US';
var identifierMap = {};

/**
 * True after at least one keydown event has been received.
 */
var gotKeyDown = false;

/**
 * Returns the layout name.
 * @return {string} layout name.
 */
function getLayoutName() {
  return getKeyboardGlyphData().layoutName;
}

/**
 * Returns layout data.
 * @return {Array} Keyboard layout data.
 */
function getLayout() {
  return keyboardOverlayData['layouts'][getLayoutName()];
}

// Cache the shortcut data after it is constructed.
var shortcutDataCache;

/**
 * Returns shortcut data.
 * @return {Object} Keyboard shortcut data.
 */
function getShortcutData() {
  if (shortcutDataCache)
    return shortcutDataCache;

  shortcutDataCache = keyboardOverlayData['shortcut'];

  if (!isDisplayUIScalingEnabled()) {
    // Zoom screen in
    delete shortcutDataCache['+<>CTRL<>SHIFT'];
    // Zoom screen out
    delete shortcutDataCache['-<>CTRL<>SHIFT'];
    // Reset screen zoom
    delete shortcutDataCache['0<>CTRL<>SHIFT'];
  }

  return shortcutDataCache;
}

/**
 * Returns the keyboard overlay ID.
 * @return {string} Keyboard overlay ID.
 */
function getKeyboardOverlayId() {
  return keyboardOverlayId;
}

/**
 * Returns keyboard glyph data.
 * @return {Object} Keyboard glyph data.
 */
function getKeyboardGlyphData() {
  return keyboardOverlayData['keyboardGlyph'][getKeyboardOverlayId()];
}

/**
 * Converts a single hex number to a character.
 * @param {string} hex Hexadecimal string.
 * @return {string} Unicode values of hexadecimal string.
 */
function hex2char(hex) {
  if (!hex) {
    return '';
  }
  var result = '';
  var n = parseInt(hex, 16);
  if (n <= 0xFFFF) {
    result += String.fromCharCode(n);
  } else if (n <= 0x10FFFF) {
    n -= 0x10000;
    result += (String.fromCharCode(0xD800 | (n >> 10)) +
               String.fromCharCode(0xDC00 | (n & 0x3FF)));
  } else {
    console.error('hex2Char error: Code point out of range :' + hex);
  }
  return result;
}

var searchIsPressed = false;

/**
 * Returns a list of modifiers from the key event.
 * @param {Event} e The key event.
 * @return {Array} List of modifiers based on key event.
 */
function getModifiers(e) {
  if (!e)
    return [];

  var isKeyDown = (e.type == 'keydown');
  var keyCodeToModifier = {
    16: 'SHIFT',
    17: 'CTRL',
    18: 'ALT',
    91: 'SEARCH',
  };
  var modifierWithKeyCode = keyCodeToModifier[e.keyCode];
  var isPressed = {
      'SHIFT': e.shiftKey,
      'CTRL': e.ctrlKey,
      'ALT': e.altKey,
      'SEARCH': searchIsPressed
  };
  if (modifierWithKeyCode)
    isPressed[modifierWithKeyCode] = isKeyDown;

  searchIsPressed = isPressed['SEARCH'];

  // make the result array
  return ['SHIFT', 'CTRL', 'ALT', 'SEARCH'].filter(
      function(modifier) {
        return isPressed[modifier];
      }).sort();
}

/**
 * Returns an ID of the key.
 * @param {string} identifier Key identifier.
 * @param {number} i Key number.
 * @return {string} Key ID.
 */
function keyId(identifier, i) {
  return identifier + '-key-' + i;
}

/**
 * Returns an ID of the text on the key.
 * @param {string} identifier Key identifier.
 * @param {number} i Key number.
 * @return {string} Key text ID.
 */
function keyTextId(identifier, i) {
  return identifier + '-key-text-' + i;
}

/**
 * Returns an ID of the shortcut text.
 * @param {string} identifier Key identifier.
 * @param {number} i Key number.
 * @return {string} Key shortcut text ID.
 */
function shortcutTextId(identifier, i) {
  return identifier + '-shortcut-text-' + i;
}

/**
 * Returns true if |list| contains |e|.
 * @param {Array} list Container list.
 * @param {string} e Element string.
 * @return {boolean} Returns true if the list contains the element.
 */
function contains(list, e) {
  return list.indexOf(e) != -1;
}

/**
 * Returns a list of the class names corresponding to the identifier and
 * modifiers.
 * @param {string} identifier Key identifier.
 * @param {Array} modifiers List of key modifiers.
 * @return {Array} List of class names corresponding to specified params.
 */
function getKeyClasses(identifier, modifiers) {
  var classes = ['keyboard-overlay-key'];
  for (var i = 0; i < modifiers.length; ++i) {
    classes.push(MODIFIER_TO_CLASS[modifiers[i]]);
  }

  if ((identifier == '2A' && contains(modifiers, 'SHIFT')) ||
      (identifier == '1D' && contains(modifiers, 'CTRL')) ||
      (identifier == '38' && contains(modifiers, 'ALT')) ||
      (identifier == 'E0 5B' && contains(modifiers, 'SEARCH'))) {
    classes.push('pressed');
    classes.push(IDENTIFIER_TO_CLASS[identifier]);
  }
  return classes;
}

/**
 * Returns true if a character is a ASCII character.
 * @param {string} c A character to be checked.
 * @return {boolean} True if the character is an ASCII character.
 */
function isAscii(c) {
  var charCode = c.charCodeAt(0);
  return 0x00 <= charCode && charCode <= 0x7F;
}

/**
 * Returns a remapped identiifer based on the preference.
 * @param {string} identifier Key identifier.
 * @return {string} Remapped identifier.
 */
function remapIdentifier(identifier) {
  return identifierMap[identifier] || identifier;
}

/**
 * Returns a label of the key.
 * @param {string} keyData Key glyph data.
 * @param {Array} modifiers Key Modifier list.
 * @return {string} Label of the key.
 */
function getKeyLabel(keyData, modifiers) {
  if (!keyData) {
    return '';
  }
  if (keyData.label) {
    return keyData.label;
  }
  var keyLabel = '';
  for (var j = 1; j <= 9; j++) {
    var pos = keyData['p' + j];
    if (!pos) {
      continue;
    }
    keyLabel = hex2char(pos);
    if (!keyLabel) {
      continue;
     }
    if (isAscii(keyLabel) &&
        getShortcutData()[getAction(keyLabel, modifiers)]) {
      break;
    }
  }
  return keyLabel;
}

/**
 * Returns a normalized string used for a key of shortcutData.
 *
 * Examples:
 *   keyCode: 'd', modifiers: ['CTRL', 'SHIFT'] => 'd<>CTRL<>SHIFT'
 *   keyCode: 'alt', modifiers: ['ALT', 'SHIFT'] => 'ALT<>SHIFT'
 *
 * @param {string} keyCode Key code.
 * @param {Array} modifiers Key Modifier list.
 * @return {string} Normalized key shortcut data string.
 */
function getAction(keyCode, modifiers) {
  /** @const */ var separatorStr = '<>';
  if (keyCode.toUpperCase() in MODIFIER_TO_CLASS) {
    keyCode = keyCode.toUpperCase();
    if (keyCode in modifiers) {
      return modifiers.join(separatorStr);
    } else {
      var action = [keyCode].concat(modifiers);
      action.sort();
      return action.join(separatorStr);
    }
  }
  return [keyCode].concat(modifiers).join(separatorStr);
}

/**
 * Returns a text which displayed on a key.
 * @param {string} keyData Key glyph data.
 * @return {string} Key text value.
 */
function getKeyTextValue(keyData) {
  if (keyData.label) {
    // Do not show text on the space key.
    if (keyData.label == 'space') {
      return '';
    }
    return keyData.label;
  }

  var chars = [];
  for (var j = 1; j <= 9; ++j) {
    var pos = keyData['p' + j];
    if (pos && pos.length > 0) {
      chars.push(hex2char(pos));
    }
  }
  return chars.join(' ');
}

/**
 * Updates the whole keyboard.
 * @param {Array} modifiers Key Modifier list.
 */
function update(modifiers) {
  var instructions = $('instructions');
  if (modifiers.length == 0) {
    instructions.style.visibility = 'visible';
  } else {
    instructions.style.visibility = 'hidden';
  }

  var keyboardGlyphData = getKeyboardGlyphData();
  var shortcutData = getShortcutData();
  var layout = getLayout();
  for (var i = 0; i < layout.length; ++i) {
    var identifier = remapIdentifier(layout[i][0]);
    var keyData = keyboardGlyphData.keys[identifier];
    var classes = getKeyClasses(identifier, modifiers, keyData);
    var keyLabel = getKeyLabel(keyData, modifiers);
    var shortcutId = shortcutData[getAction(keyLabel, modifiers)];
    if (modifiers.length == 1 && modifiers[0] == 'SHIFT' &&
        identifier == '2A') {
      // Currently there is no way to identify whether the left shift or the
      // right shift is preesed from the key event, so I assume the left shift
      // key is pressed here and do not show keyboard shortcut description for
      // 'Shift - Shift' (Toggle caps lock) on the left shift key, the
      // identifier of which is '2A'.
      // TODO(mazda): Remove this workaround (http://crosbug.com/18047)
      shortcutId = null;
    }
    if (shortcutId) {
      classes.push('is-shortcut');
    }

    var key = $(keyId(identifier, i));
    key.className = classes.join(' ');

    if (!keyData) {
      continue;
    }

    var keyText = $(keyTextId(identifier, i));
    var keyTextValue = getKeyTextValue(keyData);
    if (keyTextValue) {
       keyText.style.visibility = 'visible';
    } else {
       keyText.style.visibility = 'hidden';
    }
    keyText.textContent = keyTextValue;

    var shortcutText = $(shortcutTextId(identifier, i));
    if (shortcutId) {
      shortcutText.style.visibility = 'visible';
      shortcutText.textContent = loadTimeData.getString(shortcutId);
    } else {
      shortcutText.style.visibility = 'hidden';
    }

    var format = keyboardGlyphData.keys[layout[i][0]].format;
    if (format) {
      if (format == 'left' || format == 'right') {
        shortcutText.style.textAlign = format;
        keyText.style.textAlign = format;
      }
    }
  }
}

/**
 * A callback function for onkeydown and onkeyup events.
 * @param {Event} e Key event.
 */
function handleKeyEvent(e) {
  if (!getKeyboardOverlayId()) {
    return;
  }

  // To avoid flickering as the user releases the modifier keys that were held
  // to trigger the overlay, avoid updating in response to keyup events until at
  // least one keydown event has been received.
  if (!gotKeyDown) {
    if (e.type == 'keyup') {
      return;
    } else if (e.type == 'keydown') {
      gotKeyDown = true;
    }
  }

  var modifiers = getModifiers(e);
  update(modifiers);
  KeyboardOverlayAccessibilityHelper.maybeSpeakAllShortcuts(modifiers);
  e.preventDefault();
}

/**
 * Initializes the layout of the keys.
 */
function initLayout() {
  // Add data for the caps lock key
  var keys = getKeyboardGlyphData().keys;
  if (!('3A' in keys)) {
    keys['3A'] = {label: 'caps lock', format: 'left'};
  }
  // Add data for the special key representing a disabled key
  keys['DISABLED'] = {label: 'disabled', format: 'left'};

  var layout = getLayout();
  var keyboard = document.body;
  var minX = window.innerWidth;
  var maxX = 0;
  var minY = window.innerHeight;
  var maxY = 0;
  var multiplier = 1.38 * window.innerWidth / BASE_KEYBOARD.width;
  var keyMargin = 7;
  var offsetX = 10;
  var offsetY = 7;
  for (var i = 0; i < layout.length; i++) {
    var array = layout[i];
    var identifier = remapIdentifier(array[0]);
    var x = Math.round((array[1] + offsetX) * multiplier);
    var y = Math.round((array[2] + offsetY) * multiplier);
    var w = Math.round((array[3] - keyMargin) * multiplier);
    var h = Math.round((array[4] - keyMargin) * multiplier);

    var key = document.createElement('div');
    key.id = keyId(identifier, i);
    key.className = 'keyboard-overlay-key';
    key.style.left = x + 'px';
    key.style.top = y + 'px';
    key.style.width = w + 'px';
    key.style.height = h + 'px';

    var keyText = document.createElement('div');
    keyText.id = keyTextId(identifier, i);
    keyText.className = 'keyboard-overlay-key-text';
    keyText.style.visibility = 'hidden';
    key.appendChild(keyText);

    var shortcutText = document.createElement('div');
    shortcutText.id = shortcutTextId(identifier, i);
    shortcutText.className = 'keyboard-overlay-shortcut-text';
    shortcutText.style.visilibity = 'hidden';
    key.appendChild(shortcutText);
    keyboard.appendChild(key);

    minX = Math.min(minX, x);
    maxX = Math.max(maxX, x + w);
    minY = Math.min(minY, y);
    maxY = Math.max(maxY, y + h);
  }

  var width = maxX - minX + 1;
  var height = maxY - minY + 1;
  keyboard.style.width = (width + 2 * (minX + 1)) + 'px';
  keyboard.style.height = (height + 2 * (minY + 1)) + 'px';

  var instructions = document.createElement('div');
  instructions.id = 'instructions';
  instructions.className = 'keyboard-overlay-instructions';
  instructions.style.left = ((BASE_INSTRUCTIONS.left - BASE_KEYBOARD.left) *
                             width / BASE_KEYBOARD.width + minX) + 'px';
  instructions.style.top = ((BASE_INSTRUCTIONS.top - BASE_KEYBOARD.top) *
                            height / BASE_KEYBOARD.height + minY) + 'px';
  instructions.style.width = (width * BASE_INSTRUCTIONS.width /
                              BASE_KEYBOARD.width) + 'px';
  instructions.style.height = (height * BASE_INSTRUCTIONS.height /
                               BASE_KEYBOARD.height) + 'px';

  var instructionsText = document.createElement('div');
  instructionsText.id = 'instructions-text';
  instructionsText.className = 'keyboard-overlay-instructions-text';
  instructionsText.innerHTML =
      loadTimeData.getString('keyboardOverlayInstructions');
  instructions.appendChild(instructionsText);
  var instructionsHideText = document.createElement('div');
  instructionsHideText.id = 'instructions-hide-text';
  instructionsHideText.className = 'keyboard-overlay-instructions-hide-text';
  instructionsHideText.innerHTML =
      loadTimeData.getString('keyboardOverlayInstructionsHide');
  instructions.appendChild(instructionsHideText);
  var learnMoreLinkText = document.createElement('div');
  learnMoreLinkText.id = 'learn-more-text';
  learnMoreLinkText.className = 'keyboard-overlay-learn-more-text';
  learnMoreLinkText.addEventListener('click', learnMoreClicked);
  var learnMoreLinkAnchor = document.createElement('a');
  learnMoreLinkAnchor.href =
      loadTimeData.getString('keyboardOverlayLearnMoreURL');
  learnMoreLinkAnchor.textContent =
      loadTimeData.getString('keyboardOverlayLearnMore');
  learnMoreLinkText.appendChild(learnMoreLinkAnchor);
  instructions.appendChild(learnMoreLinkText);
  keyboard.appendChild(instructions);
}

/**
 * Returns true if the device has a diamond key.
 * @return {boolean} Returns true if the device has a diamond key.
 */
function hasDiamondKey() {
  return loadTimeData.getBoolean('keyboardOverlayHasChromeOSDiamondKey');
}

/**
 * Returns true if display scaling feature is enabled.
 * @return {boolean} True if display scaling feature is enabled.
 */
function isDisplayUIScalingEnabled() {
  return loadTimeData.getBoolean('keyboardOverlayIsDisplayUIScalingEnabled');
}

/**
 * Initializes the layout and the key labels for the keyboard that has a diamond
 * key.
 */
function initDiamondKey() {
  var newLayoutData = {
    '1D': [65.0, 287.0, 60.0, 60.0],  // left Ctrl
    '38': [185.0, 287.0, 60.0, 60.0],  // left Alt
    'E0 5B': [125.0, 287.0, 60.0, 60.0],  // search
    '3A': [5.0, 167.0, 105.0, 60.0],  // caps lock
    '5B': [803.0, 6.0, 72.0, 35.0],  // lock key
    '5D': [5.0, 287.0, 60.0, 60.0]  // diamond key
  };

  var layout = getLayout();
  var powerKeyIndex = -1;
  var powerKeyId = '00';
  for (var i = 0; i < layout.length; i++) {
    var keyId = layout[i][0];
    if (keyId in newLayoutData) {
      layout[i] = [keyId].concat(newLayoutData[keyId]);
      delete newLayoutData[keyId];
    }
    if (keyId == powerKeyId)
      powerKeyIndex = i;
  }
  for (var keyId in newLayoutData)
    layout.push([keyId].concat(newLayoutData[keyId]));

  // Remove the power key.
  if (powerKeyIndex != -1)
    layout.splice(powerKeyIndex, 1);

  var keyData = getKeyboardGlyphData()['keys'];
  var newKeyData = {
    '3A': {'label': 'caps lock', 'format': 'left'},
    '5B': {'label': 'lock'},
    '5D': {'label': 'diamond', 'format': 'left'}
  };
  for (var keyId in newKeyData)
    keyData[keyId] = newKeyData[keyId];
}

/**
 * A callback function for the onload event of the body element.
 */
function init() {
  document.addEventListener('keydown', handleKeyEvent);
  document.addEventListener('keyup', handleKeyEvent);
  chrome.send('getLabelMap');
}

/**
 * Initializes the global map for remapping identifiers of modifier keys based
 * on the preference.
 * Called after sending the 'getLabelMap' message.
 * @param {Object} remap Identifier map.
 */
function initIdentifierMap(remap) {
  for (var key in remap) {
    var val = remap[key];
    if ((key in LABEL_TO_IDENTIFIER) &&
        (val in LABEL_TO_IDENTIFIER)) {
      identifierMap[LABEL_TO_IDENTIFIER[key]] =
          LABEL_TO_IDENTIFIER[val];
    } else {
      console.error('Invalid label map element: ' + key + ', ' + val);
    }
  }
  chrome.send('getInputMethodId');
}

/**
 * Initializes the global keyboad overlay ID and the layout of keys.
 * Called after sending the 'getInputMethodId' message.
 * @param {inputMethodId} inputMethodId Input Method Identifier.
 */
function initKeyboardOverlayId(inputMethodId) {
  // Libcros returns an empty string when it cannot find the keyboard overlay ID
  // corresponding to the current input method.
  // In such a case, fallback to the default ID (en_US).
  var inputMethodIdToOverlayId =
      keyboardOverlayData['inputMethodIdToOverlayId'];
  if (inputMethodId) {
    if (inputMethodId.indexOf(IME_ID_PREFIX) == 0) {
      // If the input method is a component extension IME, remove the prefix:
      //   _comp_ime_<ext_id>
      // The extension id is a hash value with 32 characters.
      inputMethodId = inputMethodId.slice(
          IME_ID_PREFIX.length + EXTENSION_ID_LEN);
    }
    keyboardOverlayId = inputMethodIdToOverlayId[inputMethodId];
  }
  if (!keyboardOverlayId) {
    console.error('No keyboard overlay ID for ' + inputMethodId);
    keyboardOverlayId = 'en_US';
  }
  while (document.body.firstChild) {
    document.body.removeChild(document.body.firstChild);
  }
  // We show Japanese layout as-is because the user has chosen the layout
  // that is quite diffrent from the physical layout that has a diamond key.
  if (hasDiamondKey() && getLayoutName() != 'J')
    initDiamondKey();
  initLayout();
  update([]);
  window.webkitRequestAnimationFrame(function() {
    chrome.send('didPaint');
  });
}

/**
 * Handles click events of the learn more link.
 * @param {Event} e Mouse click event.
 */
function learnMoreClicked(e) {
  chrome.send('openLearnMorePage');
  chrome.send('dialogClose');
  e.preventDefault();
}

document.addEventListener('DOMContentLoaded', init);
