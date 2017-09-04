// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-action-menu',
  extends: 'dialog',

  /**
   * List of all options in this action menu.
   * @private {?NodeList<!Element>}
   */
  options_: null,

  /**
   * The element which the action menu will be anchored to. Also the element
   * where focus will be returned after the menu is closed.
   * @private {?Element}
   */
  anchorElement_: null,

  /**
   * Reference to the bound window's resize listener, such that it can be
   * removed on detach.
   * @private {?Function}
   */
  onWindowResize_: null,

  hostAttributes: {
    tabindex: 0,
  },

  listeners: {
    'keydown': 'onKeyDown_',
    'tap': 'onTap_',
  },

  /** override */
  attached: function() {
    this.options_ = this.querySelectorAll('.dropdown-item');
  },

  /** override */
  detached: function() {
    this.removeResizeListener_();
  },

  /** @private */
  removeResizeListener_: function() {
    window.removeEventListener('resize', this.onWindowResize_);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTap_: function(e) {
    if (e.target == this) {
      this.close();
      e.stopPropagation();
    }
  },

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_: function(e) {
    if (e.key == 'Tab' || e.key == 'Escape') {
      this.close();
      e.preventDefault();
      return;
    }

    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp')
      return;

    var nextOption = this.getNextOption_(e.key == 'ArrowDown' ? 1 : - 1);
    if (nextOption)
      nextOption.focus();

    e.preventDefault();
  },

  /**
   * @param {number} step -1 for getting previous option (up), 1 for getting
   *     next option (down).
   * @return {?Element} The next focusable option, taking into account
   *     disabled/hidden attributes, or null if no focusable option exists.
   * @private
   */
  getNextOption_: function(step) {
    // Using a counter to ensure no infinite loop occurs if all elements are
    // hidden/disabled.
    var counter = 0;
    var nextOption = null;
    var numOptions = this.options_.length;
    var focusedIndex = Array.prototype.indexOf.call(
        this.options_, this.root.activeElement);

    do {
      focusedIndex = (numOptions + focusedIndex + step) % numOptions;
      nextOption = this.options_[focusedIndex];
      if (nextOption.disabled || nextOption.hidden)
        nextOption = null;
      counter++;
    } while (!nextOption && counter < numOptions);

    return nextOption;
  },

  /** @override */
  close: function() {
    // Removing 'resize' listener when dialog is closed.
    this.removeResizeListener_();
    HTMLDialogElement.prototype.close.call(this);
    this.anchorElement_.focus();
    this.anchorElement_ = null;
  },

  /**
   * Shows the menu anchored to the given element.
   * @param {!Element} anchorElement
   */
  showAt: function(anchorElement) {
    this.anchorElement_ = anchorElement;
    this.onWindowResize_ = this.onWindowResize_ || function() {
      if (this.open)
        this.close();
    }.bind(this);
    window.addEventListener('resize', this.onWindowResize_);

    this.showModal();

    var rect = this.anchorElement_.getBoundingClientRect();
    if (getComputedStyle(this.anchorElement_).direction == 'rtl') {
      var right = window.innerWidth - rect.left - this.offsetWidth;
      this.style.right = right + 'px';
    } else {
      var left = rect.right - this.offsetWidth;
      this.style.left = left + 'px';
    }

    // Attempt to show the menu starting from the top of the rectangle and
    // extending downwards. If that does not fit within the window, fallback to
    // starting from the bottom and extending upwards.
    var top = rect.top + this.offsetHeight <= window.innerHeight ?
        rect.top :
        rect.bottom - this.offsetHeight - Math.max(
            rect.bottom - window.innerHeight, 0);

    this.style.top = top + 'px';
  },
});
