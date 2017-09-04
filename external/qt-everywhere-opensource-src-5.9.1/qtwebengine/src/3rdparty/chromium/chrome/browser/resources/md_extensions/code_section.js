// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  var CodeSection = Polymer({
    is: 'extensions-code-section',

    properties: {
      /**
       * The code this object is displaying.
       * @type {?chrome.developerPrivate.RequestFileSourceResponse}
       */
      code: {
        type: Object,
        // We initialize to null so that Polymer sees it as defined and calls
        // isMainHidden_().
        value: null,
      },

      /**
       * The string to display if no code is set.
       * @type {string}
       */
      noCodeError: String,
    },

    /**
     * Computes the content of the line numbers span, which basically just
     * contains 1\n2\n3\n... for the number of lines.
     * @return {string}
     * @private
     */
    computeLineNumbersContent_: function() {
      if (!this.code)
        return '';

      var lines = [this.code.beforeHighlight,
                   this.code.highlight,
                   this.code.afterHighlight].join('').match(/\n/g);
      var lineCount = lines ? lines.length : 0;
      var textContent = '';
      for (var i = 1; i <= lineCount; ++i)
        textContent += i + '\n';
      return textContent;
    },

    /**
     * @return {boolean}
     * @private
     */
    isMainHidden_: function() {
      return !this.code;
    },
  });

  return {CodeSection: CodeSection};
});
