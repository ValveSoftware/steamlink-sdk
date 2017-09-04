// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;

  /**
   * SupervisedUserLearnMore class.
   * Encapsulated handling of the 'Learn more...' overlay page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function SupervisedUserLearnMoreOverlay() {
    Page.call(this, 'supervisedUserLearnMore',
              loadTimeData.getString('supervisedUserLearnMoreTitle'),
              'supervised-user-learn-more');
  };

  cr.addSingletonGetter(SupervisedUserLearnMoreOverlay);

  SupervisedUserLearnMoreOverlay.prototype = {
    // Inherit from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('supervised-user-learn-more-done').onclick = function(event) {
        cr.ui.pageManager.PageManager.closeOverlay();
      };
    },
  };

  // Export
  return {
    SupervisedUserLearnMoreOverlay: SupervisedUserLearnMoreOverlay,
  };
});
