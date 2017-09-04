// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Modal dialog base component.
   * @param {!print_preview.MetricsContext} metricsContext Metrics
   *     context to record usage statistics.
   * @constructor
   * @extends {print_preview.Component}
   */
  function Overlay(metricsContext) {
    print_preview.Component.call(this);

    /** @private {!print_preview.MetricsContext} */
    this.metricsContext_ = metricsContext;
  };

  Overlay.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {!print_preview.MetricsContext} */
    get metricsContext() {
      return this.metricsContext_;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.getElement().addEventListener('webkitTransitionEnd', function f(e) {
        if (e.target == e.currentTarget && e.propertyName == 'opacity' &&
            e.target.classList.contains('transparent')) {
          setIsVisible(e.target, false);
        }
      });

      this.getElement().addEventListener('keydown', function f(e) {
        // Escape pressed -> cancel the dialog.
        if (!e.shiftKey && !e.ctrlKey && !e.altKey && !e.metaKey) {
          if (e.keyCode == 27) {
            e.stopPropagation();
            e.preventDefault();
            this.cancel();
          } else if (e.keyCode == 13) {
            var activeElementTag = document.activeElement ?
                document.activeElement.tagName.toUpperCase() : '';
            if (activeElementTag != 'BUTTON' && activeElementTag != 'SELECT') {
              if (this.onEnterPressedInternal()) {
                e.stopPropagation();
                e.preventDefault();
              }
            }
          }
        }
      }.bind(this));

      this.tracker.add(
          this.getChildElement('.page > .close-button'),
          'click',
          this.cancel.bind(this));

      this.tracker.add(
          this.getElement(), 'click', this.onOverlayClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.page'),
          'webkitAnimationEnd',
          this.onAnimationEnd_.bind(this));
    },

    /** @return {boolean} Whether the component is visible. */
    getIsVisible: function() {
      return !this.getElement().classList.contains('transparent');
    },

    /** @param {boolean} isVisible Whether the component is visible. */
    setIsVisible: function(isVisible) {
      if (this.getIsVisible() == isVisible)
        return;
      if (isVisible) {
        setIsVisible(this.getElement(), true);
        setTimeout(function(element) {
          element.classList.remove('transparent');
        }.bind(this, this.getElement()), 0);
      } else {
        this.getElement().classList.add('transparent');
      }
      this.onSetVisibleInternal(isVisible);
    },

    /** Closes the dialog. */
    cancel: function() {
      this.setIsVisible(false);
      this.onCancelInternal();
    },

    /**
     * @param {boolean} isVisible Whether the component is visible.
     * @protected
     */
    onSetVisibleInternal: function(isVisible) {},

    /** @protected */
    onCancelInternal: function() {},

    /**
     * @return {boolean} Whether the event was handled.
     * @protected
     */
    onEnterPressedInternal: function() {
      return false;
    },

    /**
     * Called when the overlay is clicked. Pulses the page.
     * @param {Event} e Contains the element that was clicked.
     * @private
     */
    onOverlayClick_: function(e) {
      if (e.target && e.target.classList.contains('overlay'))
        e.target.querySelector('.page').classList.add('pulse');
    },

    /**
     * Called when an animation ends on the page.
     * @param {Event} e Contains the target done animating.
     * @private
     */
    onAnimationEnd_: function(e) {
      if (e.target && e.animationName == 'pulse')
        e.target.classList.remove('pulse');
    }
  };

  // Export
  return {
    Overlay: Overlay
  };
});
