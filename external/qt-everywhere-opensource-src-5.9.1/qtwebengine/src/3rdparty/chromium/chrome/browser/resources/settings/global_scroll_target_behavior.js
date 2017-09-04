// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview |GlobalScrollTargetBehavior| allows an element to be aware of
 * the global scroll target. |scrollTarget| will be populated async by
 * |setGlobalScrollTarget|. |setGlobalScrollTarget| should only be called once.
 */

cr.define('settings', function() {
  var scrollTargetResolver = new PromiseResolver();

  /** @polymerBehavior */
  var GlobalScrollTargetBehavior = {
    properties: {
      /**
       * Read only property for the scroll target.
       * @type {HTMLElement}
       */
      scrollTarget: {
        type: Object,
        readOnly: true,
      },
    },

    /** @override */
    attached: function() {
      scrollTargetResolver.promise.then(this._setScrollTarget.bind(this));
    },
  };

  /**
   * This should only be called once.
   * @param {HTMLElement} scrollTarget
   */
  var setGlobalScrollTarget = function(scrollTarget) {
    scrollTargetResolver.resolve(scrollTarget);
  };

  return {
    GlobalScrollTargetBehavior: GlobalScrollTargetBehavior,
    setGlobalScrollTarget: setGlobalScrollTarget,
  };
});
