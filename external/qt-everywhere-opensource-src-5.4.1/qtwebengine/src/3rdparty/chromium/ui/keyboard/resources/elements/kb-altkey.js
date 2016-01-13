// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Filter out mouse/touch movements internal to this node. When moving
 * inside a node, the event should be filter out.
 * @param {Node} node The accent key node which receives event.
 * @param {event} event A pointer move event.
 * @return {boolean} True if event is external to node.
 */
 function isRelevantEvent(node, event) {
  return !(node.compareDocumentPosition(event.relatedTarget)
    & Node.DOCUMENT_POSITION_CONTAINED_BY);
};
Polymer('kb-altkey', {
  over: function(event) {
    if (isRelevantEvent(this, event)) {
      // Dragging over an accent key is equivalent to pressing on the accent
      // key.
      this.fire('key-down', {});
    }
  },

  out: function(event) {
    if (isRelevantEvent(this, event)) {
      this.classList.remove('active');
    }
  },

  up: function(event) {
    var detail = {
      char: this.charValue
    };
    this.fire('key-up', detail);
  },

  // TODO(bshe): kb-altkey should extend from kb-key-base.
  autoRelease: function() {
  },

  /**
   * Character value associated with the key. Typically, the value is a
   * single character, but may be multi-character in cases like a ".com"
   * button.
   * @return {string}
   */
   get charValue() {
    return this.char || this.textContent;
  }
});

