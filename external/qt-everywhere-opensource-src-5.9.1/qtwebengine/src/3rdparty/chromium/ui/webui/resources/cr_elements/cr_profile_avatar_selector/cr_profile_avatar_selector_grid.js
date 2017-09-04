// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector-grid' is an accessible control for
 * profile avatar icons that allows keyboard navigation with all arrow keys.
 */

Polymer({
  is: 'cr-profile-avatar-selector-grid',

  behaviors: [
    Polymer.IronMenubarBehavior,
  ],

  /**
   * Handler that is called when the up arrow key is pressed.
   * @param {CustomEvent} event A key combination event.
   * @private
   */
  _onUpKey: function(event) {
    this.moveFocusRow_(-1);
    event.detail.keyboardEvent.preventDefault();
  },

  /**
   * Handler that is called when the down arrow key is pressed.
   * @param {CustomEvent} event A key combination event.
   * @private
   */
  _onDownKey: function(event) {
    this.moveFocusRow_(1);
    event.detail.keyboardEvent.preventDefault();
  },

  /**
   * Handler that is called when the esc key is pressed.
   * @param {CustomEvent} event A key combination event.
   * @private
   */
  _onEscKey: function(event) {
    // Override the original behavior by doing nothing.
  },

  /**
   * Focuses an item on the same column as the currently focused item and on a
   * row below or above the focus row by the given offset. Focus wraps if
   * necessary.
   * @param {number} offset
   * @private
   */
  moveFocusRow_: function(offset) {
    var length = this.items.length;
    var style = getComputedStyle(this);
    var avatarSpacing =
        parseInt(style.getPropertyValue('--avatar-spacing'), 10);
    var avatarSize = parseInt(style.getPropertyValue('--avatar-size'), 10);
    var rowSize = Math.floor(this.clientWidth / (avatarSpacing + avatarSize));
    var rows = Math.ceil(length / rowSize);
    var gridSize = rows * rowSize;
    var focusIndex = this.indexOf(this.focusedItem);
    for (var i = offset; Math.abs(i) <= rows; i += offset) {
      var item = this.items[(focusIndex + i * rowSize + gridSize) % gridSize];
      if (!item)
        continue;
      this._setFocusedItem(item);

      assert(Polymer.dom(item).getOwnerRoot().activeElement == item);
      return;
    }
  }
});
