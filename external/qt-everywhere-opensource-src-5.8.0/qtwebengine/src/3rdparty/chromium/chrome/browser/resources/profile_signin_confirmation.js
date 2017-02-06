// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('profile_signin_confirmation', function() {
  'use strict';

  function initialize() {
    var args = JSON.parse(chrome.getVariableValue('dialogArguments'));
    $('dialog-message').textContent = loadTimeData.getStringF(
        'dialogMessage', args.username);
    $('dialog-prompt').textContent = loadTimeData.getStringF(
        'dialogPrompt', args.username);
    $('create-button').addEventListener('click', function() {
      chrome.send('createNewProfile');
    });
    $('continue-button').addEventListener('click', function() {
      chrome.send('continue');
    });
    $('cancel-button').addEventListener('click', function() {
      chrome.send('cancel');
    });

    if (args.promptForNewProfile) {
      $('continue-button').innerText =
          loadTimeData.getStringF('continueButtonText');
    } else {
      $('create-button').hidden = true;
      $('dialog-prompt').hidden = true;
      $('continue-button').innerText =
          loadTimeData.getStringF('okButtonText');
      // Right-align the buttons when only "OK" and "Cancel" are showing.
      $('button-row').style['text-align'] = 'end';
    }

    if (args.hideTitle)
      $('dialog-title').hidden = true;
  }

  return {
    initialize: initialize
  };
});

document.addEventListener('DOMContentLoaded',
                          profile_signin_confirmation.initialize);
