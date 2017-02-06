// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_page_manager_observer.js">
<include src="../uber/uber_utils.js">

(function() {
  var HelpPage = help.HelpPage;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * DOMContentLoaded handler, sets up the page.
   */
  function load() {
    PageManager.register(HelpPage.getInstance());

    if (help.ChannelChangePage) {
      PageManager.registerOverlay(help.ChannelChangePage.getInstance(),
                                  HelpPage.getInstance());
    }
    PageManager.addObserver(new uber.PageManagerObserver());
    PageManager.initialize(HelpPage.getInstance());
    uber.onContentFrameLoaded();

    var pageName = PageManager.getPageNameFromPath();
    // Still update history so that chrome://help/nonexistant redirects
    // appropriately to chrome://help/. If the URL matches, updateHistory
    // will avoid adding the extra state.
    var updateHistory = true;
    PageManager.showPageByName(pageName, updateHistory, {replaceState: true});
  }

  document.addEventListener('DOMContentLoaded', load);

  /**
   * Listener for the |beforeunload| event.
   */
  window.onbeforeunload = function() {
    PageManager.willClose();
  };

  /**
   * Listener for the |popstate| event.
   * @param {Event} e The |popstate| event.
   */
  window.onpopstate = function(e) {
    var pageName = PageManager.getPageNameFromPath();
    PageManager.setState(pageName, location.hash, e.state);
  };
})();
