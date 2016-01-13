// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Setup handlers for the minimize and close topbar buttons.
 */
function initializeHandlers() {
  $('minimize-button').addEventListener('click', function(e) {
    e.preventDefault();
    chrome.app.window.current().minimize();
  });

  $('minimize-button').addEventListener('mousedown', function(e) {
    e.preventDefault();
  });

  $('close-button').addEventListener('click', function() {
    window.close();
  });

  $('close-button').addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
}

window.addEventListener('DOMContentLoaded', initializeHandlers);
