// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Queries the document for an element with a matching id.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {?Element} The element with that id.
 */
var $ = function(id) {
  return document.getElementById(id);
}

function logIfError() {
  if (chrome.runtime.lastError) {
    console.log(chrome.runtime.lastError);
  }
}

function insertText(text) {
  chrome.virtualKeyboardPrivate.insertText(text, logIfError);
}

function MoveCursor(swipe_direction, swipe_flags) {
  chrome.virtualKeyboardPrivate.moveCursor(swipe_direction, swipe_flags);
}

function sendKeyEvent(event) {
  chrome.virtualKeyboardPrivate.sendKeyEvent(event, logIfError);
}

(function(scope) {
  var keyboardLocked_ = false;

  /**
   * Check the lock state of virtual keyboard.
   * @return {boolean} True if virtual keyboard is locked.
   */
  function keyboardLocked() {
    return keyboardLocked_;
  }

  /**
   * Lock or unlock virtual keyboard.
   * @param {boolean} lock Whether or not to lock the virtual keyboard.
   */
  function lockKeyboard(lock) {
    keyboardLocked_ = lock;
    chrome.virtualKeyboardPrivate.lockKeyboard(lock);
  }

  scope.keyboardLocked = keyboardLocked;
  scope.lockKeyboard = lockKeyboard;
})(this);

function hideKeyboard() {
  lockKeyboard(false);
  chrome.virtualKeyboardPrivate.hideKeyboard(logIfError);
}

function keyboardLoaded() {
  chrome.virtualKeyboardPrivate.keyboardLoaded(logIfError);
}

function getKeyboardConfig(callback) {
  chrome.virtualKeyboardPrivate.getKeyboardConfig(function (config) {
    callback(config);
  });
}

chrome.virtualKeyboardPrivate.onTextInputBoxFocused.addListener(
  function (inputContext) {
    $('keyboard').inputTypeValue = inputContext.type;
  }
);
