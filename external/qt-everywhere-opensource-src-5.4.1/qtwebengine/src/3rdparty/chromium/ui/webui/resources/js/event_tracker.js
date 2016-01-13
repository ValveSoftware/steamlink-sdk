// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview EventTracker is a simple class that manages the addition and
 *  removal of DOM event listeners. In particular, it keeps track of all
 *  listeners that have been added and makes it easy to remove some or all of
 *  them without requiring all the information again. This is particularly
 *  handy when the listener is a generated function such as a lambda or the
 *  result of calling Function.bind.
 */

// Use an anonymous function to enable strict mode just for this file (which
// will be concatenated with other files when embedded in Chrome)
var EventTracker = (function() {
  'use strict';

  /**
   *  Create an EventTracker to track a set of events.
   *  EventTracker instances are typically tied 1:1 with other objects or
   *  DOM elements whose listeners should be removed when the object is disposed
   *  or the corresponding elements are removed from the DOM.
   *  @constructor
   */
  function EventTracker() {
    /**
     *  @type {Array.<EventTracker.Entry>}
     *  @private
     */
    this.listeners_ = [];
  }

  /**
   * The type of the internal tracking entry.
   *  @typedef {{node: !Node,
   *            eventType: string,
   *            listener: Function,
   *            capture: boolean}}
   */
  EventTracker.Entry;

  EventTracker.prototype = {
    /**
     * Add an event listener - replacement for Node.addEventListener.
     * @param {!Node} node The DOM node to add a listener to.
     * @param {string} eventType The type of event to subscribe to.
     * @param {Function} listener The listener to add.
     * @param {boolean} capture Whether to invoke during the capture phase.
     */
    add: function(node, eventType, listener, capture) {
      var h = {
        node: node,
        eventType: eventType,
        listener: listener,
        capture: capture
      };
      this.listeners_.push(h);
      node.addEventListener(eventType, listener, capture);
    },

    /**
     * Remove any specified event listeners added with this EventTracker.
     * @param {!Node} node The DOM node to remove a listener from.
     * @param {?string} eventType The type of event to remove.
     */
    remove: function(node, eventType) {
      this.listeners_ = this.listeners_.filter(function(h) {
        if (h.node == node && (!eventType || (h.eventType == eventType))) {
          EventTracker.removeEventListener_(h);
          return false;
        }
        return true;
      });
    },

    /**
     * Remove all event listeners added with this EventTracker.
     */
    removeAll: function() {
      this.listeners_.forEach(EventTracker.removeEventListener_);
      this.listeners_ = [];
    }
  };

  /**
   * Remove a single event listener given it's tracker entry.  It's up to the
   * caller to ensure the entry is removed from listeners_.
   * @param {EventTracker.Entry} h The entry describing the listener to remove.
   * @private
   */
  EventTracker.removeEventListener_ = function(h) {
    h.node.removeEventListener(h.eventType, h.listener, h.capture);
  };

  return EventTracker;
})();

