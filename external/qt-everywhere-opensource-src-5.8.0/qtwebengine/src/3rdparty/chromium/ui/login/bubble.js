// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bubble implementation.
 */

// TODO(xiyuan): Move this into shared.
cr.define('cr.ui', function() {
  /**
   * Creates a bubble div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var Bubble = cr.ui.define('div');

  /**
   * Bubble key codes.
   * @enum {number}
   */
  var KeyCodes = {
    TAB: 'U+0009',
    ENTER: 'Enter',
    ESC: 'U+001B',
    SPACE: 'U+0020'
  };

  /**
   * Bubble attachment side.
   * @enum {string}
   */
  Bubble.Attachment = {
    RIGHT: 'bubble-right',
    LEFT: 'bubble-left',
    TOP: 'bubble-top',
    BOTTOM: 'bubble-bottom'
  };

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Anchor element for this bubble.
    anchor_: undefined,

    // If defined, sets focus to this element once bubble is closed. Focus is
    // set to this element only if there's no any other focused element.
    elementToFocusOnHide_: undefined,

    // With help of these elements we create closed artificial tab-cycle through
    // bubble elements.
    firstBubbleElement_: undefined,
    lastBubbleElement_: undefined,

    // Whether to hide bubble when key is pressed.
    hideOnKeyPress_: true,

    /** @override */
    decorate: function() {
      this.docKeyDownHandler_ = this.handleDocKeyDown_.bind(this);
      this.selfClickHandler_ = this.handleSelfClick_.bind(this);
      this.ownerDocument.addEventListener('click',
                                          this.handleDocClick_.bind(this));
      this.ownerDocument.addEventListener('keydown',
                                          this.docKeyDownHandler_);
      window.addEventListener('blur', this.handleWindowBlur_.bind(this));
      this.addEventListener('webkitTransitionEnd',
                            this.handleTransitionEnd_.bind(this));
      // Guard timer for 200ms + epsilon.
      ensureTransitionEndEvent(this, 250);
    },

    /**
     * Element that should be focused on hide.
     * @type {HTMLElement}
     */
    set elementToFocusOnHide(value) {
      this.elementToFocusOnHide_ = value;
    },

    /**
     * Element that should be focused on shift-tab of first bubble element
     * to create artificial closed tab-cycle through bubble.
     * Usually close-button.
     * @type {HTMLElement}
     */
    set lastBubbleElement(value) {
      this.lastBubbleElement_ = value;
    },

    /**
     * Element that should be focused on tab of last bubble element
     * to create artificial closed tab-cycle through bubble.
     * Same element as first focused on bubble opening.
     * @type {HTMLElement}
     */
    set firstBubbleElement(value) {
      this.firstBubbleElement_ = value;
    },

    /**
     * Whether to hide bubble when key is pressed.
     * @type {boolean}
     */
    set hideOnKeyPress(value) {
      this.hideOnKeyPress_ = value;
    },

    /**
     * Whether to hide bubble when clicked inside bubble element.
     * Default is true.
     * @type {boolean}
     */
    set hideOnSelfClick(value) {
      if (value)
        this.removeEventListener('click', this.selfClickHandler_);
      else
        this.addEventListener('click', this.selfClickHandler_);
    },

    /**
     * Handler for click event which prevents bubble auto hide.
     * @private
     */
    handleSelfClick_: function(e) {
      // Allow clicking on [x] button.
      if (e.target && e.target.classList.contains('close-button'))
        return;
      e.stopPropagation();
    },

    /**
     * Sets the attachment of the bubble.
     * @param {!Attachment} attachment Bubble attachment.
     */
    setAttachment_: function(attachment) {
      for (var k in Bubble.Attachment) {
        var v = Bubble.Attachment[k];
        this.classList.toggle(v, v == attachment);
      }
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!Object} pos Bubble position (left, top, right, bottom in px).
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     specified position it should be displayed).
     * @param {HTMLElement} opt_content Content to show in bubble.
     *     If not specified, bubble element content is shown.
     * @private
     */
    showContentAt_: function(pos, attachment, opt_content) {
      this.style.top = this.style.left = this.style.right = this.style.bottom =
          'auto';
      for (var k in pos) {
        if (typeof pos[k] == 'number')
          this.style[k] = pos[k] + 'px';
      }
      if (opt_content !== undefined) {
        this.innerHTML = '';
        this.appendChild(opt_content);
      }
      this.setAttachment_(attachment);
      this.hidden = false;
      this.classList.remove('faded');
    },

    /**
     * Shows the bubble for given anchor element. Bubble content is not cleared.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showForElement: function(el, attachment, opt_offset, opt_padding) {
      this.showContentForElement(
          el, attachment, undefined, opt_offset, opt_padding);
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {HTMLElement} opt_content Content to show in bubble.
     *     If not specified, bubble element content is shown.
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its width/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showContentForElement: function(el, attachment, opt_content,
                                    opt_offset, opt_padding) {
      /** @const */ var ARROW_OFFSET = 25;
      /** @const */ var DEFAULT_PADDING = 18;

      if (opt_padding == undefined)
        opt_padding = DEFAULT_PADDING;

      var origin = cr.ui.login.DisplayManager.getPosition(el);
      var offset = opt_offset == undefined ?
          [Math.min(ARROW_OFFSET, el.offsetWidth / 2),
           Math.min(ARROW_OFFSET, el.offsetHeight / 2)] :
          [opt_offset, opt_offset];

      var pos = {};
      if (isRTL()) {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
        }
      } else {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
        }
      }

      this.anchor_ = el;
      this.showContentAt_(pos, attachment, opt_content);
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {string} text Text content to show in bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its weight/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showTextForElement: function(el, text, attachment,
                                 opt_offset, opt_padding) {
      var span = this.ownerDocument.createElement('span');
      span.textContent = text;
      this.showContentForElement(el, attachment, span, opt_offset, opt_padding);
    },

    /**
     * Hides the bubble.
     */
    hide: function() {
      if (!this.classList.contains('faded'))
        this.classList.add('faded');
    },

    /**
     * Hides the bubble anchored to the given element (if any).
     * @param {!Object} el Anchor element.
     */
    hideForElement: function(el) {
      if (!this.hidden && this.anchor_ == el)
        this.hide();
    },

    /**
     * Handler for faded transition end.
     * @private
     */
    handleTransitionEnd_: function(e) {
      if (this.classList.contains('faded')) {
        this.hidden = true;
        if (this.elementToFocusOnHide_)
          this.elementToFocusOnHide_.focus();
      }
    },

    /**
     * Handler of document click event.
     * @private
     */
    handleDocClick_: function(e) {
      // Ignore clicks on anchor element.
      if (e.target == this.anchor_)
        return;

      if (!this.hidden)
        this.hide();
    },

    /**
     * Handle of document keydown event.
     * @private
     */
    handleDocKeyDown_: function(e) {
      if (this.hidden)
        return;

      if (this.hideOnKeyPress_) {
        this.hide();
        return;
      }
      // Artificial tab-cycle.

      if (e.keyIdentifier == KeyCodes.TAB && e.shiftKey == true &&
          e.target == this.firstBubbleElement_) {
        this.lastBubbleElement_.focus();
        e.preventDefault();
      }
      if (e.keyIdentifier == KeyCodes.TAB && e.shiftKey == false &&
          e.target == this.lastBubbleElement_) {
        this.firstBubbleElement_.focus();
        e.preventDefault();
      }
      // Close bubble on ESC or on hitting spacebar or Enter at close-button.
      if (e.keyIdentifier == KeyCodes.ESC ||
          ((e.keyIdentifier == KeyCodes.ENTER ||
            e.keyIdentifier == KeyCodes.SPACE) &&
             e.target && e.target.classList.contains('close-button')))
        this.hide();
    },

    /**
     * Handler of window blur event.
     * @private
     */
    handleWindowBlur_: function(e) {
      if (!this.hidden)
        this.hide();
    }
  };

  return {
    Bubble: Bubble
  };
});
