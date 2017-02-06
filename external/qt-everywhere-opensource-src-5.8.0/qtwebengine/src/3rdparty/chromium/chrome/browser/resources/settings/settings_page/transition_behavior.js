// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds convenience functions to control native Web Animations. The
 * implementation details may change as Chrome support evolves.
 * @polymerBehavior
 */
var TransitionBehavior = {
  ready: function() {
    /**
     * @type {!Object<!Animation>}
     * Map of running animations by animation name. Animation names are
     * arbitrary but used to prevent multiple animations of the same type (e.g.,
     * expand/collapse section) from running simultaneously.
     */
    this.animations = {};

    /**
     * @private {!Object<!Promise>}
     * Map of Promises for each running animation. Key names and existence
     * should correspond with |animations|.
     */
    this.promises_ = {};
  },

  /**
   * Calls el.animate(keyframes, opt_options). Returns a Promise which is
   * resolved when the transition ends, or rejected when the transition is
   * canceled. If an animation with the same name is in progress, that
   * animation is finished immediately before creating the new animation.
   * @param {string} name Name of the animation, used to finish this animation
   *     when playing the same type of animation again.
   * @param {!HTMLElement} el The element to animate.
   * @param {!Array|!Object} keyframes Keyframes, as in Element.animate.
   * @param {number|!KeyframeEffectOptions=} opt_options Options, as in
   *     Element.animate.
   * @return {!Promise} A promise which is resolved when the animation finishes
   *     or rejected if the animation is canceled.
   */
  animateElement: function(name, el, keyframes, opt_options) {
    if (this.animations[name])
      this.animations[name].finish();

    var animation = el.animate(keyframes, opt_options);
    this.animations[name] = animation;

    this.promises_[name] = new Promise(function(resolve, reject) {
      animation.addEventListener('finish', function() {
        this.animations[name] = undefined;
        resolve();
      }.bind(this));

      animation.addEventListener('cancel', function() {
        this.animations[name] = undefined;
        reject();
      }.bind(this));
    }.bind(this));
    return this.promises_[name];
  },

  /**
   * Finishes the ongoing named animation, waits for the animation's Promise
   * to be resolved, then calls the callback.
   * @param {string} name Name of the animation passed to animateElement.
   * @param {function()=} opt_callback Called once the animation finishes.
   */
  finishAnimation: function(name, opt_callback) {
    if (opt_callback)
      this.promises_[name].then(opt_callback);
    this.animations[name].finish();
  },

  /**
   * Cancels the ongoing named animation, waits for the animation's Promise
   * to be rejected, then calls the callback.
   * @param {string} name Name of the animation passed to animateElement.
   * @param {function()=} opt_callback Called once the animation cancels.
   */
  cancelAnimation: function(name, opt_callback) {
    if (opt_callback)
      this.promises_[name].catch(opt_callback);
    this.animations[name].cancel();
  },
};
