// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]" section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 */
Polymer({
  is: 'settings-section',

  properties: {
    /**
     * The current active route.
     */
    currentRoute: Object,

    /**
     * The section is expanded to a full-page view when this property matches
     * currentRoute.section.
     *
     * The section name must match the name specified in settings_router.js.
     */
    section: {
      type: String,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: String,
  },
});
