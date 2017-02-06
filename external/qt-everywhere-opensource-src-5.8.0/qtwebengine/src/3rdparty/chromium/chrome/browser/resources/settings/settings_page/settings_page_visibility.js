// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Behavior controlling the visibility of Settings pages.
 *
 * Example:
 *   behaviors: [SettingsPageVisibility],
 */

/**
 * Set this to true in tests before loading the page (e.g. in preLoad()) so that
 * pages do not initially get created. Set this to false BEFORE modifying
 * pageVisibility. NOTE: Changing this value after the DOM is loaded will not
 * trigger a visibility change, pageVisibility must be modified to trigger data
 * binding events.
 * @type {boolean}
 */
var settingsHidePagesByDefaultForTest;

/** @polymerBehavior */
var SettingsPageVisibility = {
  properties: {
    /**
     * Dictionary defining page visibility. If not set for a page, visibility
     * will default to true, unless settingsHidePagesByDefaultForTest is set
     * in which case visibility defaults to false.
     * @type {Object<boolean>}
     */
    pageVisibility: {
      type: Object,
      value: function() { return {}; },
    },
  },

  /**
   * @param {boolean} visibility
   * @return {boolean}
   */
  showPage: function(visibility) {
    if (settingsHidePagesByDefaultForTest)
      return visibility === true;
    return visibility !== false;
  },
};
