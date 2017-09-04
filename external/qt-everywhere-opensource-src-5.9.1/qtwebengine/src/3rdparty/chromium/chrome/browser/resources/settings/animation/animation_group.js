// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings.animation', function() {
  'use strict';

  /**
   * An AnimationGroup manages a set of animations, handles any styling setup or
   * cleanup, and provides a Promise for chaining actions on finish or cancel.
   * This abstracts out all these details so UI elements can simply create an
   * object rather than individually track the state of every element, style and
   * web animation. AnimationGroups may compose web animations and other
   * AnimationGroups.
   * @interface
   */
  function AnimationGroup() {}

  AnimationGroup.prototype = {
    /**
     * Sets up and plays the animation(s).
     * @return {!Promise} Convenient reference to |finished| for chaining.
     */
    play: assertNotReached,

    /**
     * If animations are still playing, immediately finishes them and resolves
     * |finished| with the |true| value.
     */
    finish: assertNotReached,

    /**
     * If animations are still playing, immediately cancels them and resolves
     * |finished| with the |false| value.
     */
    cancel: assertNotReached,

    /**
     * Resolved with a success value once the AnimationGroup finishes (true) or
     * is canceled (false).
     * @type {?Promise<boolean>}
     */
    finished: null,
  };

  return {
    AnimationGroup: AnimationGroup,
  };
});
