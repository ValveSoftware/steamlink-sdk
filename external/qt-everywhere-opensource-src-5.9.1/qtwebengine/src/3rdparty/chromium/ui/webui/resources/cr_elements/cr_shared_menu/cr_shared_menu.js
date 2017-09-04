// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Same as paper-menu-button's custom easing cubic-bezier param. */
var SLIDE_CUBIC_BEZIER = 'cubic-bezier(0.3, 0.95, 0.5, 1)';

Polymer({
  is: 'cr-shared-menu',

  properties: {
    menuOpen: {
      type: Boolean,
      observer: 'menuOpenChanged_',
      value: false,
      notify: true,
    },

    /**
     * The contextual item that this menu was clicked for, e.g. the data used to
     * render an item in an <iron-list> or <dom-repeat>.
     * @type {?Object}
     */
    itemData: {
      type: Object,
      value: null,
    },

    openAnimationConfig: {
      type: Object,
      value: function() {
        return [{
          name: 'fade-in-animation',
          timing: {
            delay: 50,
            duration: 200
          }
        }, {
          name: 'paper-menu-grow-width-animation',
          timing: {
            delay: 50,
            duration: 150,
            easing: SLIDE_CUBIC_BEZIER
          }
        }, {
          name: 'paper-menu-grow-height-animation',
          timing: {
            delay: 100,
            duration: 275,
            easing: SLIDE_CUBIC_BEZIER
          }
        }];
      }
    },

    closeAnimationConfig: {
      type: Object,
      value: function() {
        return [{
          name: 'fade-out-animation',
          timing: {
            duration: 150
          }
        }];
      }
    }
  },

  listeners: {
    'dropdown.iron-overlay-canceled': 'onOverlayCanceled_',
  },

  /**
   * The last anchor that was used to open a menu. It's necessary for toggling.
   * @private {?Element}
   */
  lastAnchor_: null,

  /** @private {?function(!Event)} */
  keyHandler_: null,

  /** @override */
  attached: function() {
    window.addEventListener('resize', this.closeMenu.bind(this));
    this.keyHandler_ = this.onCaptureKeyDown_.bind(this);
    this.$.menu.addEventListener('keydown', this.keyHandler_, true);
  },

  /** @override */
  detached: function() {
    this.$.menu.removeEventListener('keydown', this.keyHandler_, true);
  },

  /** Closes the menu. */
  closeMenu: function() {
    if (this.root.activeElement == null) {
      // Something else has taken focus away from the menu. Do not attempt to
      // restore focus to the button which opened the menu.
      this.$.dropdown.restoreFocusOnClose = false;
    }
    this.menuOpen = false;
  },

  /**
   * Opens the menu at the anchor location.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object=} opt_itemData The contextual item's data.
   */
  openMenu: function(anchor, opt_itemData) {
    if (this.lastAnchor_ == anchor && this.menuOpen)
      return;

    if (this.menuOpen)
      this.closeMenu();

    this.itemData = opt_itemData || null;
    this.lastAnchor_ = anchor;
    this.$.dropdown.restoreFocusOnClose = true;
    this.$.menu.selected = -1;

    // Move the menu to the anchor.
    this.$.dropdown.positionTarget = anchor;
    this.menuOpen = true;
  },

  /**
   * Toggles the menu for the anchor that is passed in.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object=} opt_itemData The contextual item's data.
   */
  toggleMenu: function(anchor, opt_itemData) {
    if (anchor == this.lastAnchor_ && this.menuOpen)
      this.closeMenu();
    else
      this.openMenu(anchor, opt_itemData);
  },

  /**
   * Close the menu when tab is pressed. Note that we must
   * explicitly add a capture event listener to do this as iron-menu-behavior
   * eats all key events during bubbling. See
   * https://github.com/PolymerElements/iron-menu-behavior/issues/56.
   * This will move focus to the next focusable element before/after the
   * anchor.
   * @private
   */
  onCaptureKeyDown_: function(e) {
    if (Polymer.IronA11yKeysBehavior.keyboardEventMatchesKeys(e, 'tab')) {
      // Need to refocus the anchor synchronously so that the tab event takes
      // effect on it.
      this.$.dropdown.restoreFocusOnClose = false;
      this.lastAnchor_.focus();
      this.closeMenu();
    }
  },

  /**
   * Ensure the menu is reset properly when it is closed by the dropdown (eg,
   * clicking outside).
   * @private
   */
  menuOpenChanged_: function() {
    if (!this.menuOpen) {
      this.itemData = null;
      this.lastAnchor_ = null;
    }
  },

  /**
   * Prevent focus restoring when tapping outside the menu. This stops the
   * focus moving around unexpectedly when closing the menu with the mouse.
   * @param {CustomEvent} e
   * @private
   */
  onOverlayCanceled_: function(e) {
    if (e.detail.type == 'tap')
      this.$.dropdown.restoreFocusOnClose = false;
  },
});
