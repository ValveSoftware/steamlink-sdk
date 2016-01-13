// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-altkey-container', {
  resetActiveElement: function() {
    var activeAccentKeySet = this.querySelector('#' + this.keyset);
    var offset = activeAccentKeySet.offset;
    var element = activeAccentKeySet.firstElementChild;
    while (offset) {
      element = element.nextElementSibling;
      offset--;
    }
    element.classList.add('active');
  },
  up: function(detail) {
    this.hidden = true;
    this.resetActiveElement();
    this.keyset = null;
  },
  out: function(event) {
    // Check if touch is outside container boundaries.
    if(!(this.compareDocumentPosition(event.relatedTarget)
        & Node.DOCUMENT_POSITION_CONTAINED_BY)) {
      this.hidden = true;
    }
  },

  hiddenChanged: function() {
    this.fire('stateChange', {
      state: 'candidatePopupVisibility',
      value: !!this.hidden
    });
  },
});
