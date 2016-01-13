// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var FocusManager = cr.ui.FocusManager;

  function ExtensionFocusManager() {
    FocusManager.disableMouseFocusOnButtons();
  }

  cr.addSingletonGetter(ExtensionFocusManager);

  ExtensionFocusManager.prototype = {
    __proto__: FocusManager.prototype,

    /** @override */
    getFocusParent: function() {
      var overlay = extensions.ExtensionSettings.getCurrentOverlay();
      return overlay || $('extension-settings');
    },
  };

  return {
    ExtensionFocusManager: ExtensionFocusManager,
  };
});
