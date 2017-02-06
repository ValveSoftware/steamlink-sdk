// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Same as paper-menu-button's custom easing cubic-bezier param. */
var SLIDE_CUBIC_BEZIER = 'cubic-bezier(0.3, 0.95, 0.5, 1)';
var FADE_CUBIC_BEZIER = 'cubic-bezier(0.4, 0, 0.2, 1)';

Polymer({
  is: 'cr-shared-menu',

  properties: {
    menuOpen: {
      type: Boolean,
      value: false,
    },

    /**
     * The contextual item that this menu was clicked for.
     *  e.g. the data used to render an item in an <iron-list> or <dom-repeat>
     * @type {?Object}
     */
    itemData: {
      type: Object,
      value: null,
    },
  },

  /**
   * Current animation being played, or null if there is none.
   * @type {?Animation}
   * @private
   */
  animation_: null,

  /**
   * The last anchor that was used to open a menu. It's necessary for toggling.
   * @type {?Element}
   */
  lastAnchor_: null,

  /**
   * Adds listeners to the window in order to dismiss the menu on resize and
   * when escape is pressed.
   */
  attached: function() {
    window.addEventListener('resize', this.closeMenu.bind(this));
    window.addEventListener('keydown', function(e) {
      // Escape button on keyboard
      if (e.keyCode == 27)
        this.closeMenu();
    }.bind(this));
  },

  /** Closes the menu. */
  closeMenu: function() {
    if (!this.menuOpen)
      return;
    // If there is a open menu animation going, cancel it and start closing.
    this.cancelAnimation_();
    this.menuOpen = false;
    this.itemData = null;
    this.animation_ = this.animateClose_();
    this.animation_.addEventListener('finish', function() {
      this.style.display = 'none';
      // Reset the animation for the next time the menu opens.
      this.cancelAnimation_();
    }.bind(this));
  },

  /**
   * Opens the menu at the anchor location.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object} itemData The contextual item's data.
   */
  openMenu: function(anchor, itemData) {
    this.menuOpen = true;
    this.style.display = 'block';
    this.itemData = itemData;
    this.lastAnchor_ = anchor;

    // Move the menu to the anchor.
    var anchorRect = anchor.getBoundingClientRect();
    var parentRect = this.offsetParent.getBoundingClientRect();

    var left = (isRTL() ? anchorRect.left : anchorRect.right) - parentRect.left;
    var top = anchorRect.top - parentRect.top;

    cr.ui.positionPopupAtPoint(left, top, this, cr.ui.AnchorType.BEFORE);

    // Handle the bottom of the screen.
    if (this.getBoundingClientRect().top != anchorRect.top) {
      var bottom = anchorRect.bottom - parentRect.top;
      cr.ui.positionPopupAtPoint(left, bottom, this, cr.ui.AnchorType.BEFORE);
    }

    this.$.menu.focus();

    this.cancelAnimation_();
    this.animation_ = this.animateOpen_();
  },

  /**
   * Toggles the menu for the anchor that is passed in.
   * @param {!Element} anchor The location to display the menu.
   * @param {!Object} itemData The contextual item's data.
   */
  toggleMenu: function(anchor, itemData) {
    // If there is an animation going (e.g. user clicks too fast), cancel it and
    // start the new action.
    this.cancelAnimation_();
    if (anchor == this.lastAnchor_ && this.menuOpen)
      this.closeMenu();
    else
      this.openMenu(anchor, itemData);
  },

  /** @private */
  cancelAnimation_: function() {
    if (this.animation_) {
      this.animation_.cancel();
      this.animation_ = null;
    }
  },

  /**
   * @param {!Array<!KeyframeEffect>} effects
   * @return {!Animation}
   */
  playEffects: function(effects) {
    /** @type {function(new:Object, !Array<!KeyframeEffect>)} */
    window.GroupEffect;

    /** @type {{play: function(Object): !Animation}} */
    document.timeline;

    return document.timeline.play(new window.GroupEffect(effects));
  },

  /**
   * Slide-in animation when opening the menu. The animation configuration is
   * the same as paper-menu-button except for a shorter delay time.
   * @private
   * @return {!Animation}
   */
  animateOpen_: function() {
    var rect = this.getBoundingClientRect();
    var height = rect.height;
    var width = rect.width;

    var fadeIn = new KeyframeEffect(/** @type {Animatable} */(this), [{
      'opacity': '0'
    }, {
      'opacity': '1'
    }], /** @type {!KeyframeEffectOptions} */({
      delay: 50,
      duration: 200,
      easing: FADE_CUBIC_BEZIER,
      fill: 'both'
    }));

    var growHeight = new KeyframeEffect(/** @type {Animatable} */(this), [{
      height: (height / 2) + 'px'
    }, {
      height: height + 'px'
    }], /** @type {!KeyframeEffectOptions} */({
      delay: 50,
      duration: 275,
      easing: SLIDE_CUBIC_BEZIER,
      fill: 'both'
    }));

    var growWidth = new KeyframeEffect(/** @type {Animatable} */(this), [{
      width: (width / 2) + 'px'
    }, {
      width: width + 'px'
    }], /** @type {!KeyframeEffectOptions} */({
      delay: 50,
      duration: 150,
      easing: SLIDE_CUBIC_BEZIER,
      fill: 'both'
    }));

    return this.playEffects([fadeIn, growHeight, growWidth]);
  },

  /**
   * Slide-out animation when closing the menu. The animation configuration is
   * the same as paper-menu-button.
   * @private
   * @return {!Animation}
   */
  animateClose_: function() {
    var rect = this.getBoundingClientRect();
    var height = rect.height;
    var width = rect.width;

    var fadeOut = new KeyframeEffect(/** @type {Animatable} */(this), [{
      'opacity': '1'
    }, {
      'opacity': '0'
    }], /** @type {!KeyframeEffectOptions} */({
      duration: 150,
      easing: FADE_CUBIC_BEZIER,
      fill: 'both'
    }));

    var shrinkHeight = new KeyframeEffect(/** @type {Animatable} */(this), [{
      height: height + 'px',
      transform: 'translateY(0)'
    }, {
      height: height / 2 + 'px',
      transform: 'translateY(-20px)'
    }], /** @type {!KeyframeEffectOptions} */({
      duration: 200,
      easing: 'ease-in',
      fill: 'both'
    }));

    var shrinkWidth = new KeyframeEffect(/** @type {Animatable} */(this), [{
      width: width + 'px'
    }, {
      width: width - (width / 20) + 'px'
    }], /** @type {!KeyframeEffectOptions} */({
      delay: 100,
      duration: 50,
      easing: SLIDE_CUBIC_BEZIER,
      fill: 'both'
    }));

    return this.playEffects([fadeOut, shrinkHeight, shrinkWidth]);
  },
});
