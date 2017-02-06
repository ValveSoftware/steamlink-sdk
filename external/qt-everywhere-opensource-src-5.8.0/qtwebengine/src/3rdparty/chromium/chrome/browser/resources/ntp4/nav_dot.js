// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Nav dot
 * This is the class for the navigation controls that appear along the bottom
 * of the NTP.
 */

cr.define('ntp', function() {
  'use strict';

  /**
   * Creates a new navigation dot.
   * @param {ntp.TilePage} page The associated TilePage.
   * @param {string} title The title of the navigation dot.
   * @param {boolean} titleIsEditable If true, the title can be changed.
   * @param {boolean} animate If true, animates into existence.
   * @constructor
   * @extends {HTMLLIElement}
   */
  function NavDot(page, title, titleIsEditable, animate) {
    var dot = cr.doc.createElement('li');
    dot.__proto__ = NavDot.prototype;
    dot.initialize(page, title, titleIsEditable, animate);

    return dot;
  }

  NavDot.prototype = {
    __proto__: HTMLLIElement.prototype,

    initialize: function(page, title, titleIsEditable, animate) {
      this.className = 'dot';
      this.setAttribute('role', 'button');

      this.page_ = page;

      var selectionBar = this.ownerDocument.createElement('div');
      selectionBar.className = 'selection-bar';
      this.appendChild(selectionBar);

      // TODO(estade): should there be some limit to the number of characters?
      this.input_ = this.ownerDocument.createElement('input');
      this.input_.setAttribute('spellcheck', false);
      this.input_.value = title;
      // Take the input out of the tab-traversal focus order.
      this.input_.disabled = true;
      this.appendChild(this.input_);

      this.displayTitle = title;
      this.titleIsEditable_ = titleIsEditable;

      this.addEventListener('keydown', this.onKeyDown_);
      this.addEventListener('click', this.onClick_);
      this.addEventListener('dblclick', this.onDoubleClick_);
      this.dragWrapper_ = new cr.ui.DragWrapper(this, this);
      this.addEventListener('webkitTransitionEnd', this.onTransitionEnd_);

      this.input_.addEventListener('blur', this.onInputBlur_.bind(this));
      this.input_.addEventListener('mousedown',
                                   this.onInputMouseDown_.bind(this));
      this.input_.addEventListener('keydown', this.onInputKeyDown_.bind(this));

      if (animate) {
        this.classList.add('small');
        var self = this;
        window.setTimeout(function() {
          self.classList.remove('small');
        }, 0);
      }
    },

    /**
     * @return {ntp.TilePage} The associated TilePage.
     */
    get page() {
      return this.page_;
    },

    /**
     * Sets/gets the display title.
     * @type {string} title The display name for this nav dot.
     */
    get displayTitle() {
      return this.title;
    },
    set displayTitle(title) {
      this.title = this.input_.value = title;
    },

    /**
     * Removes the dot from the page. If |opt_animate| is truthy, we first
     * transition the element to 0 width.
     * @param {boolean=} opt_animate Whether to animate the removal or not.
     */
    remove: function(opt_animate) {
      if (opt_animate)
        this.classList.add('small');
      else
        this.parentNode.removeChild(this);
    },

    /**
     * Navigates the card slider to the page for this dot.
     */
    switchToPage: function() {
      ntp.getCardSlider().selectCardByValue(this.page_, true);
    },

    /**
     * Handler for keydown event on the dot.
     * @param {Event} e The KeyboardEvent.
     */
    onKeyDown_: function(e) {
      if (e.key == 'Enter') {
        this.onClick_(e);
        e.stopPropagation();
      }
    },

    /**
     * Clicking causes the associated page to show.
     * @param {Event} e The click event.
     * @private
     */
    onClick_: function(e) {
      this.switchToPage();
      // The explicit focus call is necessary because of overriding the default
      // handling in onInputMouseDown_.
      if (this.ownerDocument.activeElement != this.input_)
        this.focus();

      e.stopPropagation();
    },

    /**
     * Double clicks allow the user to edit the page title.
     * @param {Event} e The click event.
     * @private
     */
    onDoubleClick_: function(e) {
      if (this.titleIsEditable_) {
        this.input_.disabled = false;
        this.input_.focus();
        this.input_.select();
      }
    },

    /**
     * Prevent mouse down on the input from selecting it.
     * @param {Event} e The click event.
     * @private
     */
    onInputMouseDown_: function(e) {
      if (this.ownerDocument.activeElement != this.input_)
        e.preventDefault();
    },

    /**
     * Handle keypresses on the input.
     * @param {Event} e The click event.
     * @private
     */
    onInputKeyDown_: function(e) {
      switch (e.key) {
        case 'Escape':  // Escape cancels edits.
          this.input_.value = this.displayTitle;
        case 'Enter':  // Fall through.
          this.input_.blur();
          break;
      }
    },

    /**
     * When the input blurs, commit the edited changes.
     * @param {Event} e The blur event.
     * @private
     */
    onInputBlur_: function(e) {
      window.getSelection().removeAllRanges();
      this.displayTitle = this.input_.value;
      ntp.saveAppPageName(this.page_, this.displayTitle);
      this.input_.disabled = true;
    },

    shouldAcceptDrag: function(e) {
      return this.page_.shouldAcceptDrag(e);
    },

    /**
     * A drag has entered the navigation dot. If the user hovers long enough,
     * we will navigate to the relevant page.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragEnter: function(e) {
      var self = this;
      function navPageClearTimeout() {
        self.switchToPage();
        self.dragNavTimeout = null;
      }
      this.dragNavTimeout = window.setTimeout(navPageClearTimeout, 500);

      this.doDragOver(e);
    },

    /**
     * A dragged element has moved over the navigation dot. Show the correct
     * indicator and prevent default handling so the <input> won't act as a drag
     * target.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragOver: function(e) {
      e.preventDefault();

      if (!this.dragWrapper_.isCurrentDragTarget)
        ntp.setCurrentDropEffect(e.dataTransfer, 'none');
      else
        this.page_.setDropEffect(e.dataTransfer);
    },

    /**
     * A dragged element has been dropped on the navigation dot. Tell the page
     * to append it.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDrop: function(e) {
      e.stopPropagation();
      var tile = ntp.getCurrentlyDraggingTile();
      if (tile && tile.tilePage != this.page_)
        this.page_.appendDraggingTile();
      // TODO(estade): handle non-tile drags.

      this.cancelDelayedSwitch_();
    },

    /**
     * The drag has left the navigation dot.
     * @param {Event} e The MouseOver event for the drag.
     * @private
     */
    doDragLeave: function(e) {
      this.cancelDelayedSwitch_();
    },

    /**
     * Cancels the timer for page switching.
     * @private
     */
    cancelDelayedSwitch_: function() {
      if (this.dragNavTimeout) {
        window.clearTimeout(this.dragNavTimeout);
        this.dragNavTimeout = null;
      }
    },

    /**
     * A transition has ended.
     * @param {Event} e The transition end event.
     * @private
     */
    onTransitionEnd_: function(e) {
      if (e.propertyName === 'max-width' && this.classList.contains('small'))
        this.parentNode.removeChild(this);
    },
  };

  return {
    NavDot: NavDot,
  };
});
