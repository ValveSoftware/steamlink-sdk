// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  Polymer({
    is: 'extensions-drop-overlay',
    created: function() {
      this.hidden = true;
      if (loadTimeData.getBoolean('offStoreInstallEnabled'))
        return;
      var dragTarget = document.documentElement;
      this.dragWrapperHandler_ =
          new extensions.DragAndDropHandler(true, dragTarget);
      dragTarget.addEventListener('extension-drag-started', function() {
        this.hidden = false;
      }.bind(this));
      dragTarget.addEventListener('extension-drag-ended', function() {
        this.hidden = true;
      }.bind(this));
      this.dragWrapper_ =
          new cr.ui.DragWrapper(dragTarget, this.dragWrapperHandler_);
    },
  });
})();
