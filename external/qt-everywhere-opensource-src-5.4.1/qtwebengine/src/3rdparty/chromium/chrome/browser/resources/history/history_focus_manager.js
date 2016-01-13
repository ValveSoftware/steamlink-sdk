// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FocusManager = cr.ui.FocusManager;

/**
 * A history-specific FocusManager implementation, which ensures that elements
 * "background" pages (i.e., those in a dialog that is not the topmost overlay)
 * do not receive focus.
 * @constructor
 */
function HistoryFocusManager() {
}

cr.addSingletonGetter(HistoryFocusManager);

HistoryFocusManager.prototype = {
  __proto__: FocusManager.prototype,

  /** @override */
  getFocusParent: function() {
    return document.querySelector('#overlay .showing') ||
        document.querySelector('menu:not([hidden])') ||
        $('history-page');
  },
};
