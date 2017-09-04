// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * AlertOverlay class
   * Encapsulated handling of a generic alert.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function AlertOverlay() {
    Page.call(this, 'alertOverlay', '', 'alertOverlay');
  }

  cr.addSingletonGetter(AlertOverlay);

  AlertOverlay.prototype = {
    // Inherit AlertOverlay from Page.
    __proto__: Page.prototype,

    /**
     * Whether the page can be shown. Used to make sure the page is only
     * shown via AlertOverlay.Show(), and not via the address bar.
     * @private
     */
    canShow_: false,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      // AlertOverlay is special in that it is not tied to one page or overlay.
      this.alwaysOnTop = true;

      var self = this;
      $('alertOverlayOk').onclick = function(event) {
        self.handleOK_();
      };

      $('alertOverlayCancel').onclick = function(event) {
        self.handleCancel_();
      };
    },

    /**
     * Handle the 'ok' button.  Clear the overlay and call the ok callback if
     * available.
     * @private
     */
    handleOK_: function() {
      PageManager.closeOverlay();
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
      PageManager.closeOverlay();
      if (this.cancelCallback != undefined) {
        this.cancelCallback.call();
      }
    },

    /**
     * The page is getting hidden. Don't let it be shown again.
     * @override
     */
    willHidePage: function() {
      this.canShow_ = false;
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
   * @param {function()} okCallback A function to be called when the user
   *     presses the ok button.  The alert window will be closed automatically.
   *     Can be undefined.
   * @param {function()} cancelCallback A function to be called when the user
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
    PageManager.showPageByName('alertOverlay', false);
  };

  // Export
  return {
    AlertOverlay: AlertOverlay
  };
});
