// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DragWrapper
 * A class for simplifying HTML5 drag and drop. Classes should use this to
 * handle the nitty gritty of nested drag enters and leaves.
 */
cr.define('cr.ui', function() {
  /**
   * Creates a DragWrapper which listens for drag target events on |target| and
   * delegates event handling to |handler|. The |handler| must implement:
   *   shouldAcceptDrag
   *   doDragEnter
   *   doDragLeave
   *   doDragOver
   *   doDrop
   */
  function DragWrapper(target, handler) {
    this.initialize(target, handler);
  }

  DragWrapper.prototype = {
    initialize: function(target, handler) {
      target.addEventListener('dragenter',
                              this.onDragEnter_.bind(this));
      target.addEventListener('dragover', this.onDragOver_.bind(this));
      target.addEventListener('drop', this.onDrop_.bind(this));
      target.addEventListener('dragleave', this.onDragLeave_.bind(this));

      this.target_ = target;
      this.handler_ = handler;
    },

    /**
     * The number of un-paired dragenter events that have fired on |this|. This
     * is incremented by |onDragEnter_| and decremented by |onDragLeave_|. This
     * is necessary because dragging over child widgets will fire additional
     * enter and leave events on |this|. A non-zero value does not necessarily
     * indicate that |isCurrentDragTarget()| is true.
     * @type {number}
     * @private
     */
    dragEnters_: 0,

    /**
     * Whether the tile page is currently being dragged over with data it can
     * accept.
     * @type {boolean}
     */
    get isCurrentDragTarget() {
      return this.target_.classList.contains('drag-target');
    },

    /**
     * Handler for dragenter events fired on |target_|.
     * @param {Event} e A MouseEvent for the drag.
     * @private
     */
    onDragEnter_: function(e) {
      if (++this.dragEnters_ == 1) {
        if (this.handler_.shouldAcceptDrag(e)) {
          this.target_.classList.add('drag-target');
          this.handler_.doDragEnter(e);
        }
      } else {
        // Sometimes we'll get an enter event over a child element without an
        // over event following it. In this case we have to still call the
        // drag over handler so that we make the necessary updates (one visible
        // symptom of not doing this is that the cursor's drag state will
        // flicker during drags).
        this.onDragOver_(e);
      }
    },

    /**
     * Thunk for dragover events fired on |target_|.
     * @param {Event} e A MouseEvent for the drag.
     * @private
     */
    onDragOver_: function(e) {
      if (!this.target_.classList.contains('drag-target'))
        return;
      this.handler_.doDragOver(e);
    },

    /**
     * Thunk for drop events fired on |target_|.
     * @param {Event} e A MouseEvent for the drag.
     * @private
     */
    onDrop_: function(e) {
      this.dragEnters_ = 0;
      if (!this.target_.classList.contains('drag-target'))
        return;
      this.target_.classList.remove('drag-target');
      this.handler_.doDrop(e);
    },

    /**
     * Thunk for dragleave events fired on |target_|.
     * @param {Event} e A MouseEvent for the drag.
     * @private
     */
    onDragLeave_: function(e) {
      if (--this.dragEnters_ > 0)
        return;

      this.target_.classList.remove('drag-target');
      this.handler_.doDragLeave(e);
    },
  };

  return {
    DragWrapper: DragWrapper
  };
});
