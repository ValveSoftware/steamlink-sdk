// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * @param {!Element} root
   * @param {?Node} boundary
   * @constructor
   * @extends {cr.ui.FocusRow}
   */
  function FocusRow(root, boundary) {
    cr.ui.FocusRow.call(this, root, boundary);
    this.addItems();
  }

  FocusRow.prototype = {
    __proto__: cr.ui.FocusRow.prototype,

    addItems: function() {
      this.destroy();

      this.addItem('name-file-link',
          'content.is-active:not(.show-progress):not(.dangerous) #name');
      assert(this.addItem('name-file-link', '#file-link'));
      assert(this.addItem('url', '#url'));
      this.addItem('show-retry', '#show');
      this.addItem('show-retry', '#retry');
      this.addItem('pause-resume', '#pause');
      this.addItem('pause-resume', '#resume');
      this.addItem('cancel', '#cancel');
      this.addItem('controlled-by', '#controlled-by a');
      this.addItem('danger-remove-discard', '#discard');
      this.addItem('restore-save', '#save');
      this.addItem('danger-remove-discard', '#danger-remove');
      this.addItem('restore-save', '#restore');
      assert(this.addItem('remove', '#remove'));

      // TODO(dbeam): it would be nice to do this asynchronously (so if multiple
      // templates get rendered we only re-add once), but Manager#updateItem_()
      // relies on the DOM being re-rendered synchronously.
      this.eventTracker.add(this.root, 'dom-change', this.addItems.bind(this));
    },
  };

  return {FocusRow: FocusRow};
});
