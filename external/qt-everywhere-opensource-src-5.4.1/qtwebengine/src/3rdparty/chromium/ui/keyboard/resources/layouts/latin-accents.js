// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var uppercaseAccents = {
  'A': ['\u00C0', // Capital A grave
        '\u00C1', // Capital A acute
        '\u00C2', // Capital A circumflex
        '\u00C3', // Capital A tilde
        '\u00C4', // Capital A diaeresis
        '\u00C5', // Capital A ring
        '\u00C6', // Capital ligature AE
        '\u0100'], // Capital A macron
  'C': ['\u00C7'], // Capital C cedilla
  'E': ['\u00C8', // Capital E grave
        '\u00C9', // Capital E acute
        '\u00CA', // Capital E circumflex
        '\u00CB', // Capital E diaeresis
        '\u0112'], // Capital E macron
  'I': ['\u00CC', // Capital I grave
        '\u00CD', // Capital I acute
        '\u00CE', // Capital I circumflex
        '\u00CF', // Capital I diaeresis
        '\u012A'], // Capital I macron
  'N': ['\u00D1'], // Capital N tilde
  'O': ['\u00D2', // Capital O grave
        '\u00D3', // Capital O acute
        '\u00D4', // Capital O circumflex
        '\u00D5', // Capital O tilde
        '\u00D6', // Capital O diaeresis
        '\u014C', // Capital O macron
        '\u0152'], // Capital ligature OE
  'S': ['\u1E9E'], // Capital sharp S
  'U': ['\u00D9', // Capital U grave
        '\u00DA', // Capital U acute
        '\u00DB', // Capital U circumflex
        '\u00DC', // Capital U diaeresis
        '\u016A'], // Capital U macron
};

var lowercaseAccents = {
  'a': ['\u00E0', // Lowercase A grave
        '\u00E1', // Lowercase A acute
        '\u00E2', // Lowercase A circumflex
        '\u00E3', // Lowercase A tilde
        '\u00E4', // Lowercase A diaeresis
        '\u00E5', // Lowercase A ring
        '\u00E6', // Lowercase ligature AE
        '\u0101'], // Lowercase A macron?
  'c': ['\u00E7'], // Lowercase C cedilla
  'e': ['\u00E8', // Lowercase E grave
        '\u00E9', // Lowercase E acute
        '\u00EA', // Lowercase E circumflex
        '\u00EB', // Lowercase E diaeresis
        '\u0113'], // Lowercase E macron
  'i': ['\u00EC', // Lowercase I grave
        '\u00ED', // Lowercase I acute
        '\u00EE', // Lowercase I circumflex
        '\u00EF', // Lowercase I diaeresis
        '\u012B'], // Lowercase I macron
  'n': ['\u00F1'], // Lowercase N tilde
  'o': ['\u00F2', // Lowercase O grave
        '\u00F3', // Lowercase O acute
        '\u00F4', // Lowercase O circumflex
        '\u00F5', // Lowercase O tilde
        '\u00F6', // Lowercase O diaeresis
        '\u00F8', // Lowercase O stroke
        '\u0153'], // Lowercase ligature OE
  's': ['\u00DF'], // Lowercase sharp S
  'u': ['\u00F9', // Lowercase U grave
        '\u00FA', // Lowercase U acute
        '\u00FB', // Lowercase U circumflex
        '\u00FC', // Lowercase U diaeresis
        '\u016B'], // Lowercase U macron
};

document.addEventListener('polymer-ready', function() {
  var altkeyMetadata = document.createElement('kb-altkey-data');
  altkeyMetadata.registerAltkeys(uppercaseAccents);
  altkeyMetadata.registerAltkeys(lowercaseAccents);
});
