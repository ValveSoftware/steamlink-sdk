// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('extensions');

/**
 * The different types of animations this helper supports.
 * @enum {number}
 */
extensions.Animation = {
  HERO: 0,
  FADE_IN: 1,
  FADE_OUT: 2,
  SCALE_DOWN: 3,
};

cr.define('extensions', function() {
  'use strict';

  /**
   * A helper object for setting entry/exit animations. Polymer's support of
   * this is pretty limited, since it doesn't allow for things like specifying
   * hero properties or nodes.
   * @param {!HTMLElement} element The parent element to set the animations on.
   *     This will be used as the page in to/fromPage.
   * @param {?HTMLElement} node The node to use for scaling and fading
   *     animations.
   * @constructor
   */
  function AnimationHelper(element, node) {
    this.element_ = element;
    this.node_ = node;
    element.animationConfig = {};
  }

  AnimationHelper.prototype = {
    /**
     * Set the entry animation for the element.
     * @param {extensions.Animation} animation
     */
    setEntryAnimation: function(animation) {
      var config;
      switch (animation) {
        case extensions.Animation.HERO:
          config = {name: 'hero-animation', id: 'hero', toPage: this.element_};
          break;
        case extensions.Animation.FADE_IN:
          assert(this.node_);
          config = {name: 'fade-in-animation', node: this.node_};
          break;
        default:
          assertNotReached();
      }
      this.element_.animationConfig.entry = [config];
    },

    /**
     * Set the exit animation for the element.
     * @param {extensions.Animation} animation
     */
    setExitAnimation: function(animation) {
      var config;
      switch (animation) {
        case extensions.Animation.HERO:
          config =
              {name: 'hero-animation', id: 'hero', fromPage: this.element_};
          break;
        case extensions.Animation.FADE_OUT:
          assert(this.node_);
          config = {name: 'fade-out-animation', node: this.node_};
          break;
        case extensions.Animation.SCALE_DOWN:
          assert(this.node_);
          config = {
            name: 'scale-down-animation',
            node: this.node_,
            transformOrigin: '50% 50%',
            axis: 'y',
          };
          break;
        default:
          assertNotReached();
      }
      this.element_.animationConfig.exit = [config];
    },
  };

  return {AnimationHelper: AnimationHelper};
});
