// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'supervised-user-learn-more' is a page that contains
 * information about what a supervised user is, what happens when a supervised
 * user is created, and a link to the help center for more information.
 */
Polymer({
  is: 'supervised-user-learn-more',

  properties: {
    /** @private {!signin.ProfileBrowserProxy} */
    browserProxy_: Object
  },

  listeners: {
    'tap': 'onTap_'
  },

  /** @override */
  created: function() {
    this.browserProxy_ = signin.ProfileBrowserProxyImpl.getInstance();
  },

  /**
   * Handles tap events from dynamically created links in the
   * supervisedUserLearnMoreText i18n string.
   * @param {!Event} event
   * @private
   */
  onTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    // Handle the tap event only if the target is a '<a>' element.
    if (element.nodeName == 'A') {
      this.browserProxy_.openUrlInLastActiveProfileBrowser(element.href);
      event.preventDefault();
    }
  },

  /**
   * Handler for the 'Done' button tap event.
   * @param {!Event} event
   * @private
   */
  onDoneTap_: function(event) {
    // Event is caught by user-manager-pages.
    this.fire('change-page', {page: 'create-user-page'});
  }
});
