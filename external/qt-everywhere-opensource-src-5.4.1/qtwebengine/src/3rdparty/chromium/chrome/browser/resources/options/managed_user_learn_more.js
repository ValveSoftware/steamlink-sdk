// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ManagedUserLearnMore class.
   * Encapsulated handling of the 'Learn more...' overlay page.
   * @constructor
   * @class
   */
  function ManagedUserLearnMoreOverlay() {
    OptionsPage.call(this, 'managedUserLearnMore',
                     loadTimeData.getString('managedUserLearnMoreTitle'),
                     'managed-user-learn-more');
  };

  cr.addSingletonGetter(ManagedUserLearnMoreOverlay);

  ManagedUserLearnMoreOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('managed-user-learn-more-done').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
    },
  };

  // Export
  return {
    ManagedUserLearnMoreOverlay: ManagedUserLearnMoreOverlay,
  };
});
