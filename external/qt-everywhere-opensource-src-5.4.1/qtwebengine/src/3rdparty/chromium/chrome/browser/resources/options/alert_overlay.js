// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * AlertOverlay class
   * Encapsulated handling of a generic alert.
   * @class
   */
  function AlertOverlay() {
    OptionsPage.call(this, 'alertOverlay', '', 'alertOverlay');
  }

  cr.addSingletonGetter(AlertOverlay);

  AlertOverlay.prototype = {
    // Inherit AlertOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Whether the page can be shown. Used to make sure the page is only
     * shown via AlertOverlay.Show(), and not via the address bar.
     * @private
     */
    canShow_: false,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('alertOverlayOk').onclick = function(event) {
        self.handleOK_();
      };

      $('alertOverlayCancel').onclick = function(event) {
        self.handleCancel_();
      };
    },

    /** @override */
    get nestingLevel() {
      // AlertOverlay is special in that it is not tied to one page or overlay.
      // Set the nesting level arbitrarily high so as to always be recognized as
      // the top-most visible page.
      return 99;
    },

    /**
     * Handle the 'ok' button.  Clear the overlay and call the ok callback if
     * available.
     * @private
     */
    handleOK_: function() {
      OptionsPage.closeOverlay();
      if (this.okCallback != undefined) {
        this.okCallback.call();
      }
    },

    /**
     * Handle the 'cancel' button.  Clear the overlay and call the cancel
     * callback if available.
     * @private
     */
    handleCancel_: function() {
      OptionsPage.closeOverlay();
      if (this.cancelCallback != undefined) {
        this.cancelCallback.call();
      }
    },

    /**
     * The page is getting hidden. Don't let it be shown again.
     */
    willHidePage: function() {
      canShow_ = false;
    },

    /** @override */
    canShowPage: function() {
      return this.canShow_;
    },
  };

  /**
   * Show an alert overlay with the given message, button titles, and
   * callbacks.
   * @param {string} title The alert title to display to the user.
   * @param {string} message The alert message to display to the user.
   * @param {string} okTitle The title of the OK button. If undefined or empty,
   *     no button is shown.
   * @param {string} cancelTitle The title of the cancel button. If undefined or
   *     empty, no button is shown.
   * @param {function} okCallback A function to be called when the user presses
   *     the ok button.  The alert window will be closed automatically.  Can be
   *     undefined.
   * @param {function} cancelCallback A function to be called when the user
   *     presses the cancel button.  The alert window will be closed
   *     automatically.  Can be undefined.
   */
  AlertOverlay.show = function(
      title, message, okTitle, cancelTitle, okCallback, cancelCallback) {
    if (title != undefined) {
      $('alertOverlayTitle').textContent = title;
      $('alertOverlayTitle').style.display = 'block';
    } else {
      $('alertOverlayTitle').style.display = 'none';
    }

    if (message != undefined) {
      $('alertOverlayMessage').textContent = message;
      $('alertOverlayMessage').style.display = 'block';
    } else {
      $('alertOverlayMessage').style.display = 'none';
    }

    if (okTitle != undefined && okTitle != '') {
      $('alertOverlayOk').textContent = okTitle;
      $('alertOverlayOk').style.display = 'block';
    } else {
      $('alertOverlayOk').style.display = 'none';
    }

    if (cancelTitle != undefined && cancelTitle != '') {
      $('alertOverlayCancel').textContent = cancelTitle;
      $('alertOverlayCancel').style.display = 'inline';
    } else {
      $('alertOverlayCancel').style.display = 'none';
    }

    var alertOverlay = AlertOverlay.getInstance();
    alertOverlay.okCallback = okCallback;
    alertOverlay.cancelCallback = cancelCallback;
    alertOverlay.canShow_ = true;

    // Intentionally don't show the URL in the location bar as we don't want
    // people trying to navigate here by hand.
    OptionsPage.showPageByName('alertOverlay', false);
  };

  // Export
  return {
    AlertOverlay: AlertOverlay
  };
});
