// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function makeImageSet(url1x, url2x) {
  return '-webkit-image-set(url(' + url1x + ') 1x, url(' + url2x + ') 2x)';
}

function initialize() {
  if (loadTimeData.getBoolean('allowAccessRequests')) {
    $('request-access-button').onclick = function(event) {
      $('request-access-button').hidden = true;
      sendCommand('request');
    };
  } else {
    $('request-access-button').hidden = true;
  }
  var avatarURL1x = loadTimeData.getString('avatarURL1x');
  var avatarURL2x = loadTimeData.getString('avatarURL2x');
  var custodianName = loadTimeData.getString('custodianName');
  if (custodianName) {
    $('custodians-information').hidden = false;
    if (avatarURL1x) {
      $('custodian-avatar-img').style.content =
          makeImageSet(avatarURL1x, avatarURL2x);
    }
    $('custodian-name').innerHTML = custodianName;
    $('custodian-email').innerHTML = loadTimeData.getString('custodianEmail');
    var secondAvatarURL1x = loadTimeData.getString('secondAvatarURL1x');
    var secondAvatarURL2x = loadTimeData.getString('secondAvatarURL2x');
    var secondCustodianName = loadTimeData.getString('secondCustodianName');
    if (secondCustodianName) {
      $('second-custodian-information').hidden = false;
      $('second-custodian-avatar-img').hidden = false;
      if (secondAvatarURL1x) {
        $('second-custodian-avatar-img').style.content =
            makeImageSet(secondAvatarURL1x, secondAvatarURL2x);
      }
      $('second-custodian-name').innerHTML = secondCustodianName;
      $('second-custodian-email').innerHTML = loadTimeData.getString(
          'secondCustodianEmail');
    }
  }
  var showDetailsLink = loadTimeData.getString('showDetailsLink');
  $('show-details-link').hidden = !showDetailsLink;
  $('back-button').hidden = showDetailsLink;
  $('back-button').onclick = function(event) {
    sendCommand('back');
  };
  $('show-details-link').onclick = function(event) {
    $('details').hidden = false;
    $('show-details-link').hidden = true;
    $('hide-details-link').hidden = false;
    $('information-container').classList.add('hidden-on-mobile');
    $('request-access-button').classList.add('hidden-on-mobile');
  };
  $('hide-details-link').onclick = function(event) {
    $('details').hidden = true;
    $('show-details-link').hidden = false;
    $('hide-details-link').hidden = true;
    $('information-container').classList.remove('hidden-on-mobile');
    $('request-access-button').classList.remove('hidden-on-mobile');
  };
  if (loadTimeData.getBoolean('showFeedbackLink')) {
    $('feedback-link').onclick = function(event) {
      sendCommand('feedback');
    };
  } else {
    $('feedback').hidden = true;
  }
}

/**
 * Updates the interstitial to show that the request failed or was sent.
 * @param {boolean} isSuccessful Whether the request was successful or not.
 */
function setRequestStatus(isSuccessful) {
  $('block-page-message').hidden = true;
  if (isSuccessful) {
    $('request-failed-message').hidden = true;
    $('request-sent-message').hidden = false;
    $('show-details-link').hidden = true;
    $('hide-details-link').hidden = true;
    $('details').hidden = true;
    $('back-button').hidden = false;
    $('request-access-button').hidden = true;
  } else {
    $('request-failed-message').hidden = false;
    $('request-access-button').hidden = false;
  }
}

document.addEventListener('DOMContentLoaded', initialize);
