// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper class for help page, that controls focus transition between
// elements on the help page and overlays.
cr.define('help', function() {
  function HelpFocusManager() {
  }

  cr.addSingletonGetter(HelpFocusManager);

  HelpFocusManager.prototype = {
    __proto__: cr.ui.FocusManager.prototype,

    getFocusParent: function() {
      var page = help.HelpPage.getTopmostVisiblePage();
      if (!page)
        return null;
      return page.pageDiv;
    },
  };

  return {
    HelpFocusManager: HelpFocusManager,
  };
});
