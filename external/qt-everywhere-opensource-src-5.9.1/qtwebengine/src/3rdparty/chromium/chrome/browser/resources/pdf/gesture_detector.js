// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * A class that listens for touch events and produces events when these
 * touches form gestures (e.g. pinching).
 */
class GestureDetector {
  /**
   * Constructs a GestureDetector.
   * @param {!Element} element The element to monitor for touch gestures.
   */
  constructor(element) {
    this.element_ = element;

    this.element_.addEventListener(
        'touchstart', this.onTouchStart_.bind(this), { passive: false });
    this.element_.addEventListener(
        'touchmove', this.onTouch_.bind(this), { passive: true });
    this.element_.addEventListener(
        'touchend', this.onTouch_.bind(this), { passive: true });
    this.element_.addEventListener(
        'touchcancel', this.onTouch_.bind(this), { passive: true });

    this.pinchStartEvent_ = null;
    this.lastEvent_ = null;

    this.listeners_ = new Map([
      ['pinchstart', []],
      ['pinchupdate', []],
      ['pinchend', []]
    ]);
  }

  /**
   * Add a |listener| to be notified of |type| events.
   * @param {string} type The event type to be notified for.
   * @param {Function} listener The callback.
   */
  addEventListener(type, listener) {
    if (this.listeners_.has(type)) {
      this.listeners_.get(type).push(listener);
    }
  }

  /**
   * Call the relevant listeners with the given |pinchEvent|.
   * @private
   * @param {!Object} pinchEvent The event to notify the listeners of.
   */
  notify_(pinchEvent) {
    let listeners = this.listeners_.get(pinchEvent.type);

    for (let l of listeners)
      l(pinchEvent);
  }

  /**
   * The callback for touchstart events on the element.
   * @private
   * @param {!TouchEvent} event Touch event on the element.
   */
  onTouchStart_(event) {
    // We must preventDefault if there is a two finger touch. By doing so
    // native pinch-zoom does not interfere with our way of handling the event.
    if (event.touches.length == 2) {
      event.preventDefault();
      this.pinchStartEvent_ = event;
      this.lastEvent_ = event;
      this.notify_({
        type: 'pinchstart',
        center: GestureDetector.center_(event)
      });
    }
  }

  /**
   * The callback for touch move, end, and cancel events on the element.
   * @private
   * @param {!TouchEvent} event Touch event on the element.
   */
  onTouch_(event) {
    if (!this.pinchStartEvent_)
      return;

    // Check if the pinch ends with the current event.
    if (event.touches.length < 2 ||
        this.lastEvent_.touches.length !== event.touches.length) {
      let startScaleRatio = GestureDetector.pinchScaleRatio_(
          this.lastEvent_, this.pinchStartEvent_);
      let center = GestureDetector.center_(this.lastEvent_);
      let endEvent = {
        type: 'pinchend',
        startScaleRatio: startScaleRatio,
        center: center
      };
      this.pinchStartEvent_ = null;
      this.lastEvent_ = null;
      this.notify_(endEvent);
      return;
    }

    let scaleRatio = GestureDetector.pinchScaleRatio_(event, this.lastEvent_);
    let startScaleRatio = GestureDetector.pinchScaleRatio_(
        event, this.pinchStartEvent_);
    let center = GestureDetector.center_(event);
    this.notify_({
      type: 'pinchupdate',
      scaleRatio: scaleRatio,
      direction: scaleRatio > 1.0 ? 'in' : 'out',
      startScaleRatio: startScaleRatio,
      center: center
    });

    this.lastEvent_ = event;
  }

  /**
   * Computes the change in scale between this touch event
   * and a previous one.
   * @private
   * @param {!TouchEvent} event Latest touch event on the element.
   * @param {!TouchEvent} prevEvent A previous touch event on the element.
   * @return {?number} The ratio of the scale of this event and the
   *     scale of the previous one.
   */
  static pinchScaleRatio_(event, prevEvent) {
    let distance1 = GestureDetector.distance_(prevEvent);
    let distance2 = GestureDetector.distance_(event);
    return distance1 === 0 ? null : distance2 / distance1;
  }

  /**
   * Computes the distance between fingers.
   * @private
   * @param {!TouchEvent} event Touch event with at least 2 touch points.
   * @return {number} Distance between touch[0] and touch[1].
   */
  static distance_(event) {
    let touch1 = event.touches[0];
    let touch2 = event.touches[1];
    let dx = touch1.clientX - touch2.clientX;
    let dy = touch1.clientY - touch2.clientY;
    return Math.sqrt(dx * dx + dy * dy);
  }

  /**
   * Computes the midpoint between fingers.
   * @private
   * @param {!TouchEvent} event Touch event with at least 2 touch points.
   * @return {!Object} Midpoint between touch[0] and touch[1].
   */
  static center_(event) {
    let touch1 = event.touches[0];
    let touch2 = event.touches[1];
    return {
      x: (touch1.clientX + touch2.clientX) / 2,
      y: (touch1.clientY + touch2.clientY) / 2
    };
  }
};
