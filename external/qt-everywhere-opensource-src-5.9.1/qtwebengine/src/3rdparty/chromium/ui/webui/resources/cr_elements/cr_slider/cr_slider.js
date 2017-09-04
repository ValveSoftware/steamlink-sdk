// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-slider wraps a paper-slider. It maps the slider's values from a linear UI
 * range to a range of real values.  When |value| does not map exactly to a
 * tick mark, it interpolates to the nearest tick.
 *
 * Unlike paper-slider, there is no distinction between value and
 * immediateValue; when either changes, the |value| property is updated.
 */
Polymer({
  is: 'cr-slider',

  properties: {
    /** The value the slider represents and controls. */
    value: {
      type: Number,
      notify: true,
    },

    /** @type {!Array<number>} Values corresponding to each tick. */
    tickValues: Array,

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    snaps: {
      type: Boolean,
      value: false,
    },

    maxMarkers: Number,
  },

  observers: [
    'valueChanged_(value, tickValues.*)',
  ],

  /**
   * Sets the |value| property to the value corresponding to the knob position
   * after a user action.
   * @private
   */
  onSliderChange_: function() {
    this.value = this.tickValues[this.$.slider.immediateValue];
  },

  /**
   * Updates the knob position when |value| changes. If the knob is still being
   * dragged, this instead forces |value| back to the current position.
   * @private
   */
  valueChanged_: function() {
    // First update the slider settings if |tickValues| was set.
    this.$.slider.max = this.tickValues.length - 1;

    if (this.$.slider.dragging &&
        this.value != this.tickValues[this.$.slider.immediateValue]) {
      // The value changed outside cr-slider but we're still holding the knob,
      // so set the value back to where the knob was.
      // Async so we don't confuse Polymer's data binding.
      this.async(function() {
        this.value = this.tickValues[this.$.slider.immediateValue];
      });
      return;
    }

    // Convert from the public |value| to the slider index (where the knob
    // should be positioned on the slider).
    var sliderIndex = this.tickValues.indexOf(this.value);
    if (sliderIndex == -1) {
      // No exact match.
      sliderIndex = this.findNearestIndex_(this.tickValues, this.value);
    }
    this.$.slider.value = sliderIndex;
  },

  /**
   * Returns the index of the item in |arr| closest to |value|.
   * @param {!Array<number>} arr
   * @param {number} value
   * @return {number}
   * @private
   */
  findNearestIndex_: function(arr, value) {
    var closestIndex;
    var minDifference = Number.MAX_VALUE;
    for (var i = 0; i < arr.length; i++) {
      var difference = Math.abs(arr[i] - value);
      if (difference < minDifference) {
        closestIndex = i;
        minDifference = difference;
      }
    }

    assert(typeof closestIndex != 'undefined');
    return closestIndex;
  },
});
