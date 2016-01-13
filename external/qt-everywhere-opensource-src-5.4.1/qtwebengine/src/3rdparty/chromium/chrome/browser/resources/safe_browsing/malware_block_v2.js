// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sends a command message to SafeBrowsingBlockingPage::CommandReceived.
 * @param {string} cmd The command to send.
 */
function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

/**
 * Records state of the reporting checkbox.
 */
function savePreference() {
  var checkBox = $('check-report');
  if (checkBox.checked)
    sendCommand('doReport');
  else
    sendCommand('dontReport');
}

/**
 * Expands or collapses the "see more" section of the page.
 */
function seeMore() {
  if ($('see-less-text').hidden) {
    $('see-more-text').hidden = true;
    $('see-less-text').hidden = false;
    $('see-more-contents').hidden = false;
    sendCommand('expandedSeeMore');
  } else {
    $('see-more-text').hidden = false;
    $('see-less-text').hidden = true;
    $('see-more-contents').hidden = true;
  }
}

/* This sets up 4 conditions for the Field Trial.
 * The 'NoBrand' conditions don't have the Chrome/Chromium logo at the top.
 * The 'OneStep' conditions don't hide the proceed button.
 */
function setupInterstitialExperiment() {
  var condition = templateData.trialType;
  if (condition == 'cond2MalwareNoBrand' ||
      condition == 'cond4PhishingNoBrand') {
    $('logo').style.display = 'none';
  } else if (condition == 'cond5MalwareOneStep' ||
             condition == 'cond6PhishingOneStep') {
    $('see-more-contents').hidden = false;
    $('see-less-text').hidden = true;
    $('see-more-text').hidden = true;
  }
}

/**
 * Onload listener to initialize javascript handlers.
 */
document.addEventListener('DOMContentLoaded', function() {
  $('proceed-span').hidden = templateData.proceedDisabled;

  $('back').onclick = function() {
    sendCommand('takeMeBack');
  };
  $('proceed').onclick = function(e) {
    sendCommand('proceed');
  };
  $('learn-more-link').onclick = function(e) {
    sendCommand('learnMore2');
  };
  $('show-diagnostic-link').onclick = function(e) {
    sendCommand('showDiagnostic');
  };
  $('report-error-link').onclick = function(e) {
    sendCommand('reportError');
  };
  $('see-more-link').onclick = function(e) {
    seeMore();
    // preventDefaultOnPoundLinkClicks doesn't work for this link since it
    // contains <span>s, which confuse preventDefaultOnPoundLinkClicks.
    e.preventDefault();
  };
  $('check-report').onclick = savePreference;

  // All the links are handled by javascript sending commands back to the C++
  // handler, we don't want the default actions.
  preventDefaultOnPoundLinkClicks();  // From webui/js/util.js.

  setupInterstitialExperiment();

  // Allow jsdisplay elements to be visible now.
  document.documentElement.classList.remove('loading');
});
