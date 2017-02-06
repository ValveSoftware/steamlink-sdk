// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * @param {!Element} root
   * @param {?Element} boundary
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function FocusRow(root, boundary) {
    cr.ui.FocusRow.call(this, root, boundary);
  }

  FocusRow.prototype = {
    __proto__: cr.ui.FocusRow.prototype,

    /** @override */
    makeActive: function(active) {
      cr.ui.FocusRow.prototype.makeActive.call(this, active);

      // Only highlight if the row has focus.
      this.root.classList.toggle('extension-highlight',
          active && this.root.contains(document.activeElement));
    },
  };

  return {FocusRow: FocusRow};
});
