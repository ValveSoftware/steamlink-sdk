// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `cr-events` provides helpers for handling events in Chrome Polymer elements.
 *
 * Example:
 *
 *    <cr-events id="events"></cr-events>
 *
 * Usage:
 *
 *    this.$.events.forward(this.$.element, ['change']);
 */
Polymer({
  is: 'cr-events',

  /**
   * Sets up an element to forward events across the shadow boundary, for events
   * which normally stop at the root node (see http://goo.gl/WGMO9x).
   * @param {!HTMLElement} element The element to forward events from.
   * @param {!Array<string>} events The events to forward.
   */
  forward: function(element, events) {
    for (var i = 0; i < events.length; i++)
      element.addEventListener(events[i], this.forwardEvent_);
  },

  /**
   * Forwards events that don't automatically cross the shadow boundary
   * if the event should bubble.
   * @param {!Event} e The event to forward.
   * @param {*} detail Data passed when initializing the event.
   * @param {Node=} opt_sender Node that declared the handler.
   * @private
   */
  forwardEvent_: function(e, detail, opt_sender) {
    if (!e.bubbles)
      return;

    var node = e.path[e.path.length - 1];
    if (node instanceof ShadowRoot) {
      // Forward the event to the shadow host.
      e.stopPropagation();
      node.host.fire(e.type, detail, node.host, true, e.cancelable);
    }
  },
});
