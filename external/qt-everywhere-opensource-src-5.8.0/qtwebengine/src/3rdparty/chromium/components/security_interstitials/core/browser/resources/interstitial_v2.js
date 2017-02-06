// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the shared code for the new (Chrome 37) security interstitials. It is
// used for both SSL interstitials and Safe Browsing interstitials.

var expandedDetails = false;
var keyPressState = 0;

// Should match SecurityInterstitialCommands in security_interstitial_page.h
var CMD_DONT_PROCEED = 0;
var CMD_PROCEED = 1;
// Ways for user to get more information
var CMD_SHOW_MORE_SECTION = 2;
var CMD_OPEN_HELP_CENTER = 3;
var CMD_OPEN_DIAGNOSTIC = 4;
// Primary button actions
var CMD_RELOAD = 5;
var CMD_OPEN_DATE_SETTINGS = 6;
var CMD_OPEN_LOGIN = 7;
// Safe Browsing Extended Reporting
var CMD_DO_REPORT = 8;
var CMD_DONT_REPORT = 9;
var CMD_OPEN_REPORTING_PRIVACY = 10;
// Report a phishing error.
var CMD_REPORT_PHISHING_ERROR = 11;

/**
 * A convenience method for sending commands to the parent page.
 * @param {string} cmd  The command to send.
 */
function sendCommand(cmd) {
<if expr="not is_ios">
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
</if>
<if expr="is_ios">
  // TODO(crbug.com/565877): Revisit message passing for WKWebView.
  var iframe = document.createElement('IFRAME');
  iframe.setAttribute('src', 'js-command:' + cmd);
  document.documentElement.appendChild(iframe);
  iframe.parentNode.removeChild(iframe);
</if>
}

/**
 * This allows errors to be skippped by typing a secret phrase into the page.
 * @param {string} e The key that was just pressed.
 */
function handleKeypress(e) {
  var BYPASS_SEQUENCE = 'badidea';
  if (BYPASS_SEQUENCE.charCodeAt(keyPressState) == e.keyCode) {
    keyPressState++;
    if (keyPressState == BYPASS_SEQUENCE.length) {
      sendCommand(CMD_PROCEED);
      keyPressState = 0;
    }
  } else {
    keyPressState = 0;
  }
}

/**
 * This appends a piece of debugging information to the end of the warning.
 * When complete, the caller must also make the debugging div
 * (error-debugging-info) visible.
 * @param {string} title  The name of this debugging field.
 * @param {string} value  The value of the debugging field.
 */
function appendDebuggingField(title, value) {
  // The values input here are not trusted. Never use innerHTML on these
  // values!
  var spanTitle = document.createElement('span');
  spanTitle.classList.add('debugging-title');
  spanTitle.innerText = title + ': ';

  var spanValue = document.createElement('span');
  spanValue.classList.add('debugging-value');
  spanValue.innerText = value;

  var pElem = document.createElement('p');
  pElem.classList.add('debugging-content');
  pElem.appendChild(spanTitle);
  pElem.appendChild(spanValue);
  $('error-debugging-info').appendChild(pElem);
}

function toggleDebuggingInfo() {
  $('error-debugging-info').classList.toggle('hidden');
}

function setupEvents() {
  var overridable = loadTimeData.getBoolean('overridable');
  var interstitialType = loadTimeData.getString('type');
  var ssl = interstitialType == 'SSL';
  var captivePortal = interstitialType == 'CAPTIVE_PORTAL';
  var badClock = ssl && loadTimeData.getBoolean('bad_clock');
  var hidePrimaryButton = badClock && loadTimeData.getBoolean(
      'hide_primary_button');

  if (ssl) {
    $('body').classList.add(badClock ? 'bad-clock' : 'ssl');
    $('error-code').textContent = loadTimeData.getString('errorCode');
    $('error-code').classList.remove('hidden');
  } else if (captivePortal) {
    $('body').classList.add('captive-portal');
  } else {
    $('body').classList.add('safe-browsing');
  }

  if (hidePrimaryButton) {
    $('primary-button').classList.add('hidden');
  } else {
    $('primary-button').addEventListener('click', function() {
      switch (interstitialType) {
        case 'CAPTIVE_PORTAL':
          sendCommand(CMD_OPEN_LOGIN);
          break;

        case 'SSL':
          if (badClock)
            sendCommand(CMD_OPEN_DATE_SETTINGS);
          else if (overridable)
            sendCommand(CMD_DONT_PROCEED);
          else
            sendCommand(CMD_RELOAD);
          break;

        case 'SAFEBROWSING':
          sendCommand(CMD_DONT_PROCEED);
          break;

        default:
          throw 'Invalid interstitial type';
      }
    });
  }

  if (overridable) {
    // Captive portal page isn't overridable.
    $('proceed-link').addEventListener('click', function(event) {
      sendCommand(CMD_PROCEED);
    });
  } else if (!ssl) {
    $('final-paragraph').classList.add('hidden');
  }

  if (ssl && overridable) {
    $('proceed-link').classList.add('small-link');
  } else if ($('help-link')) {
    // Overridable SSL page doesn't have this link.
    $('help-link').addEventListener('click', function(event) {
      if (ssl || loadTimeData.getBoolean('phishing'))
        sendCommand(CMD_OPEN_HELP_CENTER);
      else
        sendCommand(CMD_OPEN_DIAGNOSTIC);
    });
  }

  if (captivePortal) {
    // Captive portal page doesn't have details button.
    $('details-button').classList.add('hidden');
  } else {
    $('details-button').addEventListener('click', function(event) {
      var hiddenDetails = $('details').classList.toggle('hidden');

      if (mobileNav) {
        // Details appear over the main content on small screens.
        $('main-content').classList.toggle('hidden', !hiddenDetails);
      } else {
        $('main-content').classList.remove('hidden');
      }

      $('details-button').innerText = hiddenDetails ?
          loadTimeData.getString('openDetails') :
          loadTimeData.getString('closeDetails');
      if (!expandedDetails) {
        // Record a histogram entry only the first time that details is opened.
        sendCommand(CMD_SHOW_MORE_SECTION);
        expandedDetails = true;
      }
    });
  }

  // TODO(felt): This should be simplified once the Finch trial is no longer
  // needed.
  if (interstitialType == 'SAFEBROWSING' &&
      loadTimeData.getBoolean('phishing') && $('report-error-link')) {
    $('report-error-link').addEventListener('click', function(event) {
      sendCommand(CMD_REPORT_PHISHING_ERROR);
    });
  }

  preventDefaultOnPoundLinkClicks();
  setupExtendedReportingCheckbox();
  setupSSLDebuggingInfo();
  document.addEventListener('keypress', handleKeypress);
}

document.addEventListener('DOMContentLoaded', setupEvents);
