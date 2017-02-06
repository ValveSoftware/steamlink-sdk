// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-searched-label',

  properties: {
    // The text to show in this label.
    title: String,

    // The search term to bold within the title.
    searchTerm: String,
  },

  observers: ['setSearchedTextToBold_(title, searchTerm)'],

  /**
   * Updates the page title. If a search term is specified, highlights any
   * occurrences of the search term in bold.
   * @private
   */
  setSearchedTextToBold_: function() {
    var i = 0;
    var titleElem = this.$.container;
    var titleText = this.title;

    if (this.searchTerm == '' || this.searchTerm == null) {
      titleElem.textContent = titleText;
      return;
    }

    var re = new RegExp(quoteString(this.searchTerm), 'gim');
    var match;
    titleElem.textContent = '';
    while (match = re.exec(titleText)) {
      if (match.index > i)
        titleElem.appendChild(document.createTextNode(
            titleText.slice(i, match.index)));
      i = re.lastIndex;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = titleText.substring(match.index, i);
      titleElem.appendChild(b);
    }
    if (i < titleText.length)
      titleElem.appendChild(
          document.createTextNode(titleText.slice(i)));
  },
});
