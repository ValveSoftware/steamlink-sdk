// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var FocusManager = cr.ui.FocusManager;
  var OptionsPage = options.OptionsPage;

  function OptionsFocusManager() {
  }

  cr.addSingletonGetter(OptionsFocusManager);

  OptionsFocusManager.prototype = {
    __proto__: FocusManager.prototype,

    /** @override */
    getFocusParent: function() {
      var topPage = OptionsPage.getTopmostVisiblePage().pageDiv;

      // The default page and search page include a search field that is a
      // sibling of the rest of the page instead of a child. Thus, use the
      // parent node to allow the search field to receive focus.
      if (topPage.parentNode.id == 'page-container')
        return topPage.parentNode;

      return topPage;
    },
  };

  return {
    OptionsFocusManager: OptionsFocusManager,
  };
});
