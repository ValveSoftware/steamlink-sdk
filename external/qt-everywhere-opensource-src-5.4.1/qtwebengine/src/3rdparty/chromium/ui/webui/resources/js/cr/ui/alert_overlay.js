// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('alertOverlay', function() {
  /**
   * The confirm <button>.
   * @type {HTMLButtonElement}
   */
  var okButton;

  /**
   * The cancel <button>.
   * @type {HTMLButtonElement}
   */
  var cancelButton;

  function initialize(e) {
    okButton = $('alertOverlayOk');
    cancelButton = $('alertOverlayCancel');

    // The callbacks are set to the callbacks provided in show(). Clear them
    // out when either is clicked.
    okButton.addEventListener('click', function(e) {
      assert(okButton.clickCallback);

      okButton.clickCallback(e);
      okButton.clickCallback = null;
      cancelButton.clickCallback = null;
    });
    cancelButton.addEventListener('click', function(e) {
      assert(cancelButton.clickCallback);

      cancelButton.clickCallback(e);
      okButton.clickCallback = null;
      cancelButton.clickCallback = null;
    });
  };

  /**
   * Updates the alert overlay with the given message, button titles, and
   * callbacks.
   * @param {string} title The alert title to display to the user.
   * @param {string} message The alert message to display to the user.
   * @param {string=} okTitle The title of the OK button. If undefined or empty,
   *     no button is shown.
   * @param {string=} cancelTitle The title of the cancel button. If undefined
   *     or empty, no button is shown.
   * @param {function=} okCallback A function to be called when the user presses
   *     the ok button. Can be undefined if |okTitle| is falsey.
   * @param {function=} cancelCallback A function to be called when the user
   *     presses the cancel button. Can be undefined if |cancelTitle| is falsey.
   */
  function setValues(
      title, message, okTitle, cancelTitle, okCallback, cancelCallback) {
    if (typeof title != 'undefined')
      $('alertOverlayTitle').textContent = title;
    $('alertOverlayTitle').hidden = typeof title == 'undefined';

    if (typeof message != 'undefined')
      $('alertOverlayMessage').textContent = message;
    $('alertOverlayMessage').hidden = typeof message == 'undefined';

    if (okTitle)
      okButton.textContent = okTitle;
    okButton.hidden = !okTitle;
    okButton.clickCallback = okCallback;

    if (cancelTitle)
      cancelButton.textContent = cancelTitle;
    cancelButton.hidden = !cancelTitle;
    cancelButton.clickCallback = cancelCallback;
  };

  // Export
  return {
    initialize: initialize,
    setValues: setValues
  };
});

document.addEventListener('DOMContentLoaded', alertOverlay.initialize);
