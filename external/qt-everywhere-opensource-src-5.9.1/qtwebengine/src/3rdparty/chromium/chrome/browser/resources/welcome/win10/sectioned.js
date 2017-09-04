// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sectioned', function() {
  'use strict';

  function computeClasses(isCombined) {
    if (isCombined)
      return 'section expandable expanded';
    return 'section';
  }

  function onContinue() {
    chrome.send('handleContinue');
  }

  function onOpenSettings() {
    chrome.send('handleSetDefaultBrowser');
  }

  function onToggle(app) {
    if (app.isCombined) {
      // Toggle sections.
      var sections = document.querySelectorAll('.section.expandable');
      sections.forEach(function(section) {
        section.classList.toggle('expanded');
      });
      // Toggle screenshots.
      var screenshots = document.querySelectorAll('.screenshot-image');
      screenshots.forEach(function(screenshot) {
        screenshot.classList.toggle('hidden');
      });
    }
  }

  function initialize() {
    var app = $('sectioned-app');

    // Set variables.
    // Determines if the combined variant should be displayed. The combined
    // variant includes instructions on how to pin Chrome to the taskbar.
    app.isCombined = false;

    // Set handlers.
    app.computeClasses = computeClasses;
    app.onContinue = onContinue;
    app.onOpenSettings = onOpenSettings;
    app.onToggle = onToggle.bind(this, app);


    // Asynchronously check if Chrome is pinned to the taskbar.
    cr.sendWithPromise('getPinnedToTaskbarState').then(
      function(isPinnedToTaskbar) {
        // Allow overriding of the result via a query parameter.
        // TODO(pmonette): Remove these checks when they are no longer needed.
        /** @const */ var VARIANT_KEY = 'variant';
        var VariantType = {
          DEFAULT_ONLY: 'defaultonly',
          COMBINED: 'combined'
        };
        var params = new URLSearchParams(location.search.slice(1));
        if (params.has(VARIANT_KEY)) {
          if (params.get(VARIANT_KEY) === VariantType.DEFAULT_ONLY)
            app.isCombined = false;
          else if (params.get(VARIANT_KEY) === VariantType.COMBINED)
            app.isCombined = true;
        } else {
          app.isCombined = !isPinnedToTaskbar;
        }
      });
  }

  return {
    initialize: initialize
  };
});

document.addEventListener('DOMContentLoaded', sectioned.initialize);
