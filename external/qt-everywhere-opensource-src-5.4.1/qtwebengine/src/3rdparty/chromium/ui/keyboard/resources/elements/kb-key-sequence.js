// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

  var metadata = null;

  function getKeyCodeAndModifiers(keyCodeIndex) {
    if (!metadata)
      metadata = document.createElement('kb-key-codes');
    return metadata.GetKeyCodeAndModifiers(keyCodeIndex);
  }

  Polymer('kb-key-sequence', {
    /**
     * Generates the DOM structure to replace (expand) this kb-key-sequence.
     */
    generateDom: function() {
      var replacement = document.createDocumentFragment();
      var newKeys = this.getAttribute('keys');
      var newHintTexts = this.getAttribute('hintTexts');
      var keyCodes = this.getAttribute('hintCodes') || newKeys;
      var invert = this.getAttribute('invert');
      if (newKeys) {
        if (newHintTexts && newKeys.length != newHintTexts.length) {
          console.error('keys and hintTexts do not match');
          return;
        }
        if (keyCodes && newKeys.length != keyCodes.length) {
           console.error('keys and keyCodes do not match');
          return;
        }

        for (var i = 0; i < newKeys.length; i++) {
          var key = document.createElement('kb-key');
          key.innerText = newKeys[i];
          key.accents = newKeys[i];
          if (newHintTexts)
            key.hintText = newHintTexts[i];
          var keyCodeIndex = keyCodes[i];
          if (invert) {
            key.invert = true;
            key.char = newKeys[i];
            keyCodeIndex = key.hintText;
          }
          var state = getKeyCodeAndModifiers(keyCodeIndex);
          if (state) {
            key.keyCode = state.keyCode;
            key.keyName = state.keyName;
            key.shiftModifier = state.shiftModifier;
          }
          replacement.appendChild(key);
        }
      }
      return replacement;
    }
  });
})();

