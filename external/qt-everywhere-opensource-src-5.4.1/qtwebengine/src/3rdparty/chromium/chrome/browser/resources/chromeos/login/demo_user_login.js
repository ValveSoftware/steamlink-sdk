// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo login UI.
 */

/**
 * Handles a user clicking anywhere on the screen. This will log the demo user
 * in. Yes, this actually _is the intention.
 * @param {Event} e The click event that triggered this function.
 */
onClick = function(e) {
  document.removeEventListener('click', onClick);
  e.stopPropagation();
  showLoginSpinner();
  chrome.send('launchDemoUser');
};

/**
 * Initializes the click handler.
 */
initialize = function() {
  $('page').style.opacity = 1;
  document.addEventListener('click', onClick);
  // Report back sign in UI being painted.
  window.webkitRequestAnimationFrame(function() {
    chrome.send('loginVisible', ['demo']);
  });
};

/**
 * Show the login spinner.
 */
showLoginSpinner = function() {
  // We're already logging in - don't login on click.
  document.removeEventListener('click', onClick);

  // Hide the "Click to start" and show the spinner.
  $('demo-login-text').hidden = true;
  $('login-spinner').hidden = false;
};

disableTextSelectAndDrag();
document.addEventListener('DOMContentLoaded', initialize);
