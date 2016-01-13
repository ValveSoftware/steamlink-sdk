// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var symbolAltKeys = {
  '1': ['\u00B9', // HintText 1
        '\u00BD', // Vulgar fraction 1/2
        '\u2153', // Vulgar fraction 1/3
        '\u00BC', // Vulgar fraction 1/4
        '\u215B'], // Vulgar fraction 1/8
  '2': ['\u00B2', // HintText 2
        '\u2154'], // Vulgar fraction 2/3
  '3': ['\u00B3', // HintText 3
        '\u00BE', // Vulgar fraction 3/4
        '\u215C'], // Vulgar fraction 3/8
  '4': ['\u2074'], // HintText 4
  '5': ['\u215D'], // Vulgar fraction 5/8
  '7': ['\u215E'], // Vulgar fraction 7/8
  '0': ['\u00D8', // Empty set
        '\u207F'], // HintText small n
  '$': ['\u20AC', // Euro sign
        '\u00A5', // Yen sign
        '\u00A3', // Pound sign
        '\u00A2'], // Cent sign
  '%': ['\u2030'], // Per Mille sign
  '*': ['\u2605', // Black star
        '\u2020', // Dagger
        '\u2021'], // Double dagger
  '\\': ['/'],
  '"': ['\u201C', // Left double quote
        '\u201D', // Right double quote
        '\u201E', // Double low-9 quotation mark
        '\u00AB', // Left double angle quote
        '\u00BB'], // Right double angle quote
  '-': ['_'],
  '!': ['\u00A1'], // Inverted exclamation mark
  '?': ['\u00BF'], // Inverted question mark
  '(': ['<', '{', '['],
  ')': ['>', '}', ']'],
  '.com' : ['.net','.org','.gov']
};

document.addEventListener('polymer-ready', function() {
  var altkeyMetadata = document.createElement('kb-altkey-data');
  altkeyMetadata.registerAltkeys(symbolAltKeys);
});
