/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('sync.confirmation', function() {
  'use strict';

  function onConfirm(e) {
    chrome.send('confirm');
  }

  function onUndo(e) {
    chrome.send('undo');
  }

  function onGoToSettings(e) {
    chrome.send('goToSettings');
  }

  function initialize() {
    document.addEventListener('keydown', onKeyDown);
    $('confirmButton').addEventListener('click', onConfirm);
    $('undoButton').addEventListener('click', onUndo);
    if (loadTimeData.getBoolean('isSyncAllowed')) {
      $('settingsLink').addEventListener('click', onGoToSettings);
      $('profile-picture').addEventListener('load', onPictureLoaded);
      $('syncDisabledDetails').hidden = true;
    } else {
      $('syncConfirmationDetails').hidden = true;
    }

    // Prefer using |document.body.offsetHeight| instead of
    // |document.body.scrollHeight| as it returns the correct height of the
    // even when the page zoom in Chrome is different than 100%.
    chrome.send('initializedWithSize', [document.body.offsetHeight]);
  }

  function clearFocus() {
    document.activeElement.blur();
  }

  function setUserImageURL(url) {
    if (loadTimeData.getBoolean('isSyncAllowed')) {
      $('profile-picture').src = url;
    }
  }

  function onPictureLoaded(e) {
    if (loadTimeData.getBoolean('isSyncAllowed')) {
      $('picture-container').classList.add('loaded');
    }
  }

  function onKeyDown(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK, Got It".
    if (e.key == 'Enter' &&
        !/^(A|PAPER-BUTTON)$/.test(document.activeElement.tagName)) {
      $('confirmButton').click();
      e.preventDefault();
    }
  }

  return {
    clearFocus: clearFocus,
    initialize: initialize,
    setUserImageURL: setUserImageURL
  };
});

document.addEventListener('DOMContentLoaded', sync.confirmation.initialize);
