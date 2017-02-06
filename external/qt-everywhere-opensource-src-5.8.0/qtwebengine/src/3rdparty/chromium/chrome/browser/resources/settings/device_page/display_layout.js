// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'display-layout' presents a visual representation of the layout of one or
 * more displays and allows them to be arranged.
 */

(function() {

/** @const {number} */ var MIN_VISUAL_SCALE = .01;

Polymer({
  is: 'display-layout',

  behaviors: [
    Polymer.IronResizableBehavior,
    DragBehavior,
  ],

  properties: {
    /**
     * Array of displays.
     * @type {!Array<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Array,

    /**
     * Array of display layouts.
     * @type {!Array<!chrome.system.display.DisplayLayout>}
     */
    layouts: Array,

    /**
     * Whether or not mirroring is enabled.
     * @type {boolean}
     */
    mirroring: false,

    /** @type {!chrome.system.display.DisplayUnitInfo|undefined} */
    selectedDisplay: Object,

    /**
     * The ratio of the display area div (in px) to DisplayUnitInfo.bounds.
     * @type {number}
     */
    visualScale: 1,
  },

  /** @private {!Object<chrome.system.display.Bounds>} */
  displayBoundsMap_: {},

  /** @private {!Object<chrome.system.display.DisplayLayout>} */
  layoutMap_: {},

  /**
   * The calculated bounds used for generating the div bounds.
   * @private {!Object<chrome.system.display.Bounds>}
  */
  calculatedBoundsMap_: {},

  /** @private {!{left: number, top: number}} */
  visualOffset_: {left: 0, top: 0},

  /** @override */
  attached: function() {
    // TODO(stevenjb): Remove retry once fixed:
    // https://github.com/Polymer/polymer/issues/3629
    var self = this;
    var retry = 100;  // ms
    function tryCalcVisualScale() {
      if (!self.calculateVisualScale_())
        setTimeout(tryCalcVisualScale, retry);
    }
    tryCalcVisualScale();
  },

  /** @override */
  detached: function() {
    this.initializeDrag(false);
  },

  /**
   * Called explicitly when |this.displays| and their associated |this.layouts|
   * have been fetched from chrome.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!Array<!chrome.system.display.DisplayLayout>} layouts
   */
  updateDisplays: function(displays, layouts) {
    this.displays = displays;
    this.layouts = layouts;

    this.mirroring = displays.length > 0 && !!displays[0].mirroringSourceId;

    this.displayBoundsMap_ = {};
    for (let display of this.displays)
      this.displayBoundsMap_[display.id] = display.bounds;

    this.layoutMap_ = {};
    for (let layout of this.layouts)
      this.layoutMap_[layout.id] = layout;

    this.calculatedBoundsMap_ = {};
    for (let display of this.displays)
      this.calculateBounds_(display.id, display.bounds);

    this.calculateVisualScale_();

    this.initializeDrag(
        !this.mirroring, this.$.displayArea, this.onDrag_.bind(this));
  },

  /**
   * Calculates the visual offset and scale for the display area
   * (i.e. the ratio of the display area div size to the area required to
   * contain the DisplayUnitInfo bounding boxes).
   * @return {boolean} Whether the calculation was successful.
   * @private
   */
  calculateVisualScale_() {
    var displayAreaDiv = this.$.displayArea;
    if (!displayAreaDiv || !displayAreaDiv.offsetWidth || !this.displays ||
        !this.displays.length) {
      return false;
    }

    var display = this.displays[0];
    var bounds = this.calculatedBoundsMap_[display.id];
    var displayInfoBoundingBox = {
      left: bounds.left,
      right: bounds.left + bounds.width,
      top: bounds.top,
      bottom: bounds.top + bounds.height,
    };
    var maxWidth = bounds.width;
    var maxHeight = bounds.height;
    for (let i = 1; i < this.displays.length; ++i) {
      display = this.displays[i];
      bounds = this.calculatedBoundsMap_[display.id];
      displayInfoBoundingBox.left =
          Math.min(displayInfoBoundingBox.left, bounds.left);
      displayInfoBoundingBox.right =
          Math.max(displayInfoBoundingBox.right, bounds.left + bounds.width);
      displayInfoBoundingBox.top =
          Math.min(displayInfoBoundingBox.top, bounds.top);
      displayInfoBoundingBox.bottom =
          Math.max(displayInfoBoundingBox.bottom, bounds.top + bounds.height);
      maxWidth = Math.max(maxWidth, bounds.width);
      maxHeight = Math.max(maxHeight, bounds.height);
    }

    // Create a margin around the bounding box equal to the size of the
    // largest displays.
    var displayInfoBoundsWidth = displayInfoBoundingBox.right -
        displayInfoBoundingBox.left + maxWidth * 2;
    var displayInfoBoundsHeight = displayInfoBoundingBox.bottom -
        displayInfoBoundingBox.top + maxHeight * 2;

    // Calculate the scale.
    var horizontalScale = displayAreaDiv.offsetWidth / displayInfoBoundsWidth;
    var verticalScale = displayAreaDiv.offsetHeight / displayInfoBoundsHeight;
    var scale = Math.min(horizontalScale, verticalScale);

    // Calculate the offset.
    this.visualOffset_.left = (-displayInfoBoundingBox.left + maxWidth) * scale;
    this.visualOffset_.top = (-displayInfoBoundingBox.top + maxHeight) * scale;

    // Update the scale which will trigger calls to getDivStyle_.
    this.visualScale = Math.max(MIN_VISUAL_SCALE, scale);

    return true;
  },

  /**
   * @param {string} id
   * @param {!chrome.system.display.Bounds} displayBounds
   * @param {number} visualScale
   * @return {string} The style string for the div.
   * @private
   */
  getDivStyle_: function(id, displayBounds, visualScale) {
    // This matches the size of the box-shadow or border in CSS.
    /** @const {number} */ var BORDER = 2;
    var bounds = this.calculatedBoundsMap_[id];
    var height = Math.round(bounds.height * this.visualScale) - BORDER * 2;
    var width = Math.round(bounds.width * this.visualScale) - BORDER * 2;
    var left =
        Math.round(this.visualOffset_.left + (bounds.left * this.visualScale));
    var top =
        Math.round(this.visualOffset_.top + (bounds.top * this.visualScale));
    return `height: ${height}px; width: ${width}px;` +
        ` left: ${left}px; top: ${top}px`;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  isSelected_: function(display, selectedDisplay) {
    return display.id == selectedDisplay.id;
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onSelectDisplayTap_: function(e) {
    this.fire('select-display', e.model.index);
  },

  /**
   * Recursively calculate the bounds of a display relative to its parents.
   * Caches the display bounds so that parent bounds are only calculated once.
   * TODO(stevenjb): Move this function and the maps it requires to a separate
   *     behavior which will include snapping and collisions.
   * @param {string} id
   * @param {!chrome.system.display.Bounds} bounds
   * @private
   */
  calculateBounds_: function(id, bounds) {
    if (id in this.calculatedBoundsMap_)
      return;  // Already calculated (i.e. a parent of a previous display)
    var left, top;
    var layout = this.layoutMap_[id];
    if (!layout || !layout.parentId) {
      left = -bounds.width / 2;
      top = -bounds.height / 2;
    } else {
      var parentDisplayBounds = this.displayBoundsMap_[layout.parentId];
      var parentBounds;
      if (!(layout.parentId in this.calculatedBoundsMap_))
        this.calculateBounds_(layout.parentId, parentDisplayBounds);
      parentBounds = this.calculatedBoundsMap_[layout.parentId];
      left = parentBounds.left;
      top = parentBounds.top;
      switch (layout.position) {
        case chrome.system.display.LayoutPosition.TOP:
          top -= bounds.height;
          break;
        case chrome.system.display.LayoutPosition.RIGHT:
          left += parentBounds.width;
          break;
        case chrome.system.display.LayoutPosition.BOTTOM:
          top += parentBounds.height;
          break;
        case chrome.system.display.LayoutPosition.LEFT:
          left -= bounds.height;
          break;
      }
    }
    var result = {
      left: left,
      top: top,
      width: bounds.width,
      height: bounds.height
    };
    this.calculatedBoundsMap_[id] = result;
  },

  /**
   * @param {string} id
   * @param {?DragPosition} amount
   */
  onDrag_(id, amount) {
    id = id.substr(1);  // Skip prefix

    var newBounds;
    if (!amount) {
      // TODO(stevenjb): Resolve layout and send update.
      newBounds = this.calculatedBoundsMap_[id];
    } else {
      // Make sure the dragged display is also selected.
      if (id != this.selectedDisplay.id)
        this.fire('select-display', id);

      var calculatedBounds = this.calculatedBoundsMap_[id];
      newBounds =
          /** @type {chrome.system.display.Bounds} */ (
              Object.assign({}, calculatedBounds));
      newBounds.left += Math.round(amount.x / this.visualScale);
      newBounds.top += Math.round(amount.y / this.visualScale);
      // TODO(stevenjb): Update layout.
    }
    var left =
        this.visualOffset_.left + Math.round(newBounds.left * this.visualScale);
    var top =
        this.visualOffset_.top + Math.round(newBounds.top * this.visualScale);
    var div = this.$$('#_' + id);
    div.style.left = '' + left + 'px';
    div.style.top = '' + top + 'px';
  },

});

})();
