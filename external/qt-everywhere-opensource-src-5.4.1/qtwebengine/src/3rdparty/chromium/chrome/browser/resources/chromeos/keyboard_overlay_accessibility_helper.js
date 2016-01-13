// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An object to implement keyboard overlay accessiblity.
var KeyboardOverlayAccessibilityHelper = {
  // Returns true when ChromeVox is loaded and active, false otherwise.
  cvoxIsActive: function() {
    return window.cvox && window.cvox.Api.isChromeVoxActive();
  },
  // Speaks all the shortcut with the given modifiers.
  maybeSpeakAllShortcuts: function(modifiers) {
    if (!this.cvoxIsActive())
      return;
    cvox.Api.stop();
    var keyboardGlyphData = getKeyboardGlyphData();
    var shortcutData = getShortcutData();
    var layout = getLayouts()[keyboardGlyphData.layoutName];
    var keysToShortcutText = {};
    for (var i = 0; i < layout.length; ++i) {
      var identifier = remapIdentifier(layout[i][0]);
      var keyData = keyboardGlyphData.keys[identifier];
      var keyLabel = getKeyLabel(keyData, modifiers);
      var shortcutId = shortcutData[getAction(keyLabel, modifiers)];
      var shortcutText = templateData[shortcutId];
      var keysText = modifiers.concat(keyLabel).join(' + ');
      if (shortcutText)
        keysToShortcutText[keysText] = shortcutText;
    }
    for (var keysText in keysToShortcutText) {
      this.speakShortcut_(keysText, keysToShortcutText[keysText]);
    }
  },
  // Speaks given shortcut description.
  speakShortcut_: function(keysText, shortcutText) {
    keysText = keysText.toLowerCase();  // For correct pronunciation.
    cvox.Api.speak(keysText, 1, {});
    cvox.Api.speak(shortcutText, 1, {});
  },
};
