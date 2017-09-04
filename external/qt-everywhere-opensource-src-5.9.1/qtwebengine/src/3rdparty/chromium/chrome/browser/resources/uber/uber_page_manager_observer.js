// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('uber', function() {
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * A PageManager observer that updates the uber page.
   * @constructor
   * @implements {cr.ui.pageManager.PageManager.Observer}
   */
  function PageManagerObserver() {}

  PageManagerObserver.prototype = {
    __proto__: PageManager.Observer.prototype,

    /**
     * Informs the uber page when a top-level overlay is opened or closed.
     * @param {cr.ui.pageManager.Page} page The page that is being shown or was
     *     hidden.
     * @override
     */
    onPageVisibilityChanged: function(page) {
      if (PageManager.isTopLevelOverlay(page)) {
        if (page.visible)
          uber.invokeMethodOnParent('beginInterceptingEvents');
        else
          uber.invokeMethodOnParent('stopInterceptingEvents');
      }
    },

    /**
     * Uses uber to set the title.
     * @param {string} title The title to set.
     * @override
     */
    updateTitle: function(title) {
      uber.setTitle(title);
    },

    /**
     * Pushes the current page onto the history stack, replacing the current
     * entry if appropriate.
     * @param {string} path The path of the page to push onto the stack.
     * @param {boolean} replace If true, allow no history events to be created.
     * @override
     */
    updateHistory: function(path, replace) {
      var historyFunction = replace ? uber.replaceState : uber.pushState;
      historyFunction({}, path);
    },
  };

  // Export
  return {
    PageManagerObserver: PageManagerObserver
  };
});
