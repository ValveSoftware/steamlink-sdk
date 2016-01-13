// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * ExtensionCode is an element which displays code in a styled div, and is
   * designed to highlight errors.
   */
  function ExtensionCode(div) {
    div.__proto__ = ExtensionCode.prototype;
    return div;
  }

  ExtensionCode.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Populate the content area of the code div with the given code. This will
     * highlight the erroneous section (if any).
     * @param {Object} code An object with four strings: beforeHighlight,
     *     afterHighlight, highlight, and the message. The 'highlight' strings
     *     represent the three portions of the file's content to display - the
     *     portion which is most relevant and should be emphasized (highlight),
     *     and the parts both before and after this portion. The message is the
     *     error message, which will be the mouseover hint for the highlighted
     *     region. These may be empty.
     *  @param {string} emptyMessage The message to display if the code
     *     object is empty (e.g., 'could not load code').
     */
    populate: function(code, emptyMessage) {
      // Clear any remnant content, so we don't have multiple code listed.
      this.clear();

      var sourceDiv = document.createElement('div');
      sourceDiv.classList.add('extension-code-source');
      this.appendChild(sourceDiv);

      // If there's no code, then display an appropriate message.
      if (!code ||
          (!code.highlight && !code.beforeHighlight && !code.afterHighlight)) {
        var span = document.createElement('span');
        span.textContent = emptyMessage;
        sourceDiv.appendChild(span);
        return;
      }

      var lineCount = 0;
      var createSpan = function(source, isHighlighted) {
        lineCount += source.split('\n').length - 1;
        var span = document.createElement('span');
        span.className = isHighlighted ? 'extension-code-highlighted-source' :
                                         'extension-code-normal-source';
        span.textContent = source;
        return span;
      };

      if (code.beforeHighlight)
        sourceDiv.appendChild(createSpan(code.beforeHighlight, false));

      if (code.highlight) {
        var highlightSpan = createSpan(code.highlight, true);
        highlightSpan.title = code.message;
        sourceDiv.appendChild(highlightSpan);
      }

      if (code.afterHighlight)
        sourceDiv.appendChild(createSpan(code.afterHighlight, false));

      // Make the line numbers. This should be the number of line breaks + 1
      // (the last line doesn't break, but should still be numbered).
      var content = '';
      for (var i = 1; i < lineCount + 1; ++i)
        content += i + '\n';
      var span = document.createElement('span');
      span.textContent = content;

      var linesDiv = document.createElement('div');
      linesDiv.classList.add('extension-code-line-numbers');
      linesDiv.appendChild(span);
      this.insertBefore(linesDiv, this.firstChild);
    },

    /**
     * Clears the content of the element.
     */
    clear: function() {
      while (this.firstChild)
        this.removeChild(this.firstChild);
    },

    /**
     * Scrolls to the error, if there is one. This cannot be called when the
     * div is hidden.
     */
    scrollToError: function() {
      var errorSpan = this.querySelector('.extension-code-highlighted-source');
      if (errorSpan)
        this.scrollTop = errorSpan.offsetTop - this.clientHeight / 2;
    }
  };

  // Export
  return {
    ExtensionCode: ExtensionCode
  };
});
