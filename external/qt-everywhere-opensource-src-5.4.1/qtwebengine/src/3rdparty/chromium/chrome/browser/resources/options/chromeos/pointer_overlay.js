// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * PointerOverlay class
   * Dialog that allows users to set pointer settings (touchpad/mouse).
   * @extends {SettingsDialog}
   */
  function PointerOverlay() {
    // The title is updated dynamically in the setTitle method as pointer
    // devices are discovered or removed.
    SettingsDialog.call(this, 'pointer-overlay',
                        '', 'pointer-overlay',
                        $('pointer-overlay-confirm'),
                        $('pointer-overlay-cancel'));
  }

  cr.addSingletonGetter(PointerOverlay);

  PointerOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      SettingsDialog.prototype.initializePage.call(this);
    },
  };

  /**
   * Sets the visibility state of the touchpad group.
   * @param {boolean} show True to show, false to hide.
   */
  PointerOverlay.showTouchpadControls = function(show) {
    $('pointer-section-touchpad').hidden = !show;
  };

  /**
   * Sets the visibility state of the mouse group.
   * @param {boolean} show True to show, false to hide.
   */
  PointerOverlay.showMouseControls = function(show) {
    $('pointer-section-mouse').hidden = !show;
  };

  /**
   * Updates the title of the pointer dialog.  The title is set dynamically
   * based on whether a touchpad, mouse or both are present.  The label on the
   * button that activates the overlay is also updated to stay in sync. A
   * message is displayed in the main settings page if no pointer devices are
   * available.
   * @param {string} label i18n key for the overlay title.
   */
  PointerOverlay.setTitle = function(label) {
    var button = $('pointer-settings-button');
    var noPointersLabel = $('no-pointing-devices');
    if (label.length > 0) {
      var title = loadTimeData.getString(label);
      button.textContent = title;
      button.hidden = false;
      noPointersLabel.hidden = true;
    } else {
      button.hidden = true;
      noPointersLabel.hidden = false;
    }
  };

  // Export
  return {
    PointerOverlay: PointerOverlay
  };
});
