// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('alertOverlay', function() {
  /**
   * The confirm <button>.
   * @type {HTMLElement}
   */
  var okButton;

  /**
   * The cancel <button>.
   * @type {HTMLElement}
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
   * @param {string=} opt_okTitle The title of the OK button. If undefined or
   *     empty, no button is shown.
   * @param {string=} opt_cancelTitle The title of the cancel button. If
   *     undefined or empty, no button is shown.
   * @param {function()=} opt_okCallback A function to be called when the user
   *     presses the ok button. Can be undefined if |opt_okTitle| is falsey.
   * @param {function()=} opt_cancelCallback A function to be called when the
   *     user presses the cancel button. Can be undefined if |opt_cancelTitle|
   *     is falsey.
   */
  function setValues(title, message, opt_okTitle, opt_cancelTitle,
                     opt_okCallback, opt_cancelCallback) {
    if (typeof title != 'undefined')
      $('alertOverlayTitle').textContent = title;
    $('alertOverlayTitle').hidden = typeof title == 'undefined';

    if (typeof message != 'undefined')
      $('alertOverlayMessage').textContent = message;
    $('alertOverlayMessage').hidden = typeof message == 'undefined';

    if (opt_okTitle)
      okButton.textContent = opt_okTitle;
    okButton.hidden = !opt_okTitle;
    okButton.clickCallback = opt_okCallback;

    if (opt_cancelTitle)
      cancelButton.textContent = opt_cancelTitle;
    cancelButton.hidden = !opt_cancelTitle;
    cancelButton.clickCallback = opt_cancelCallback;
  };

  // Export
  return {
    initialize: initialize,
    setValues: setValues
  };
});

document.addEventListener('DOMContentLoaded', alertOverlay.initialize);
