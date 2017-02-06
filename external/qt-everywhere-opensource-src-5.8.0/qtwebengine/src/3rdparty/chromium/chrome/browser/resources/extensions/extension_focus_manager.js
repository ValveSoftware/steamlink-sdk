// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * @constructor
   * @extends {cr.ui.FocusManager}
   */
  function ExtensionFocusManager() {}

  cr.addSingletonGetter(ExtensionFocusManager);

  ExtensionFocusManager.prototype = {
    __proto__: cr.ui.FocusManager.prototype,

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
