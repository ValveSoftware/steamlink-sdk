// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for handling display layout, specifically
 *     edge snapping and collisions.
 */

/** @polymerBehavior */
var LayoutBehavior = {
  properties: {
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
  },

  /** @private {!Map<string, chrome.system.display.Bounds>} */
  displayBoundsMap_: new Map(),

  /** @private {!Map<string, chrome.system.display.DisplayLayout>} */
  displayLayoutMap_: new Map(),

  /**
   * The calculated bounds used for generating the div bounds.
   * @private {!Map<string, chrome.system.display.Bounds>}
   */
  calculatedBoundsMap_: new Map(),

  /** @private {string} */
  dragLayoutId: '',

  /** @private {string} */
  dragParentId_: '',

  /** @private {!chrome.system.display.Bounds|undefined} */
  dragBounds_: undefined,

  /** @private {!chrome.system.display.LayoutPosition|undefined} */
  dragLayoutPosition_: undefined,

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!Array<!chrome.system.display.DisplayLayout>} layouts
   */
  initializeDisplayLayout: function(displays, layouts) {
    this.dragLayoutId = '';
    this.dragParentId_ = '';

    this.mirroring = displays.length > 0 && !!displays[0].mirroringSourceId;

    this.displayBoundsMap_.clear();
    for (let display of displays)
      this.displayBoundsMap_.set(display.id, display.bounds);

    this.displayLayoutMap_.clear();
    for (let layout of layouts)
      this.displayLayoutMap_.set(layout.id, layout);

    this.calculatedBoundsMap_.clear();
    for (let display of displays) {
      if (!this.calculatedBoundsMap_.has(display.id)) {
        let bounds = display.bounds;
        this.calculateBounds_(display.id, bounds.width, bounds.height);
      }
    }
  },

  /**
   * Called when a drag event occurs. Checks collisions and updates the layout.
   * @param {string} id
   * @param {!chrome.system.display.Bounds} newBounds The new calculated
   *     bounds for the display.
   * @return {!chrome.system.display.Bounds}
   */
  updateDisplayBounds(id, newBounds) {
    this.dragLayoutId = id;

    // Find the closest parent.
    var closestId = this.findClosest_(id, newBounds);

    // Find the closest edge.
    var closestBounds = this.getCalculatedDisplayBounds(closestId);
    var layoutPosition =
        this.getLayoutPositionForBounds_(newBounds, closestBounds);

    // Snap to the closest edge.
    var snapPos = this.snapBounds_(newBounds, closestId, layoutPosition);
    newBounds.left = snapPos.x;
    newBounds.top = snapPos.y;

    // Calculate the new bounds and delta.
    var oldBounds = this.dragBounds_ || this.getCalculatedDisplayBounds(id);
    var deltaPos = {
      x: newBounds.left - oldBounds.left,
      y: newBounds.top - oldBounds.top
    };

    // Check for collisions after snapping. This should not collide with the
    // closest parent.
    this.collideAndModifyDelta_(id, oldBounds, deltaPos);

    // If the edge changed, update and highlight it.
    if (layoutPosition != this.dragLayoutPosition_ ||
        closestId != this.dragParentId_) {
      this.dragLayoutPosition_ = layoutPosition;
      this.dragParentId_ = closestId;
      this.highlightEdge_(closestId, layoutPosition);
    }

    newBounds.left = oldBounds.left + deltaPos.x;
    newBounds.top = oldBounds.top + deltaPos.y;

    this.dragBounds_ = newBounds;

    return newBounds;
  },

  /**
   * Called when dragging ends. Sends the updated layout to chrome.
   * @param {string} id
   */
  finishUpdateDisplayBounds(id) {
    this.highlightEdge_('', undefined);  // Remove any highlights.
    if (id != this.dragLayoutId || !this.dragBounds_ ||
        !this.dragLayoutPosition_) {
      return;
    }

    var layout = this.displayLayoutMap_.get(id);

    var orphanIds;
    if (!layout || layout.parentId == '') {
      // Primary display. Set the calculated position to |dragBounds_|.
      this.setCalculatedDisplayBounds_(id, this.dragBounds_);

      // We cannot re-parent the primary display, so instead make all other
      // displays orphans and clear their calculated bounds.
      orphanIds = this.findChildren_(id, true /* recurse */);
      for (let o in orphanIds)
        this.calculatedBoundsMap_.delete(o);

      // Re-parent |dragParentId_|. It will be forced to parent to the dragged
      // display since it is the only non-orphan.
      this.reparentOrphan_(this.dragParentId_, orphanIds);
      orphanIds.splice(orphanIds.indexOf(this.dragParentId_), 1);
    } else {
      // All immediate children of |layout| will need to be re-parented.
      orphanIds = this.findChildren_(id, false /* do not recurse */);
      for (let o in orphanIds)
        this.calculatedBoundsMap_.delete(o);

      // When re-parenting to a descendant, also parent any immediate child to
      // drag display's current parent.
      var topLayout = this.displayLayoutMap_.get(this.dragParentId_);
      while (topLayout && topLayout.parentId != '') {
        if (topLayout.parentId == id) {
          topLayout.parentId = layout.parentId;
          break;
        }
        topLayout = this.displayLayoutMap_.get(topLayout.parentId);
      }

      // Re-parent the dragged display.
      layout.parentId = this.dragParentId_;
      this.updateOffsetAndPosition_(
          this.dragBounds_, this.dragLayoutPosition_, layout);
    }

    // Update any orphaned children. This may cause the dragged display to
    // be re-attached if it was attached to a child.
    this.updateOrphans_(orphanIds);

    // Send the updated layouts.
    chrome.system.display.setDisplayLayout(this.layouts, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'setDisplayLayout Error: ' + chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {string} displayId
   * @param {boolean=} opt_notest Set to true if bounds may not be set.
   * @return {!chrome.system.display.Bounds} bounds
   */
  getCalculatedDisplayBounds: function(displayId, opt_notest) {
    var bounds = this.calculatedBoundsMap_.get(displayId);
    assert(opt_notest || bounds);
    return bounds;
  },

  /**
   * @param {string} displayId
   * @param {!chrome.system.display.Bounds|undefined} bounds
   * @private
   */
  setCalculatedDisplayBounds_: function(displayId, bounds) {
    assert(bounds);
    this.calculatedBoundsMap_.set(
        displayId,
        /** @type {!chrome.system.display.Bounds} */ (
            Object.assign({}, bounds)));
  },

  /**
   * Re-parents all entries in |orphanIds| and any children.
   * @param {!Array<string>} orphanIds The list of ids affected by the move.
   * @private
   */
  updateOrphans_: function(orphanIds) {
    var orphans = orphanIds.slice();
    for (let orphan of orphanIds) {
      var newOrphans = this.findChildren_(orphan, true /* recurse */);
      // If the dragged display was re-parented to one of its children,
      // there may be duplicates so merge the lists.
      for (let o of newOrphans) {
        if (!orphans.includes(o))
          orphans.push(o);
      }
    }

    // Remove each orphan from the list as it is re-parented so that
    // subsequent orphans can be parented to it.
    while (orphans.length) {
      var orphanId = orphans.shift();
      this.reparentOrphan_(orphanId, orphans);
    }
  },

  /**
   * Re-parents the orphan to a layout that is not a member of
   * |otherOrphanIds|.
   * @param {string} orphanId The id of the orphan to re-parent.
   * @param {Array<string>} otherOrphanIds The list of ids of other orphans
   *     to ignore when re-parenting.
   * @private
   */
  reparentOrphan_: function(orphanId, otherOrphanIds) {
    var layout = this.displayLayoutMap_.get(orphanId);
    assert(layout);
    if (orphanId == this.dragId_ && layout.parentId != '') {
      this.setCalculatedDisplayBounds_(orphanId, this.dragBounds_);
      return;
    }
    var bounds = this.getCalculatedDisplayBounds(orphanId);

    // Find the closest parent.
    var newParentId = this.findClosest_(orphanId, bounds, otherOrphanIds);
    assert(newParentId != '');
    layout.parentId = newParentId;

    // Find the closest edge.
    var parentBounds = this.getCalculatedDisplayBounds(newParentId);
    var layoutPosition = this.getLayoutPositionForBounds_(bounds, parentBounds);

    // Move from the nearest corner to the desired location and get the delta.
    var cornerBounds = this.getCornerBounds_(bounds, parentBounds);
    var desiredPos = this.snapBounds_(bounds, newParentId, layoutPosition);
    var deltaPos = {
      x: desiredPos.x - cornerBounds.left,
      y: desiredPos.y - cornerBounds.top
    };

    // Check for collisions.
    this.collideAndModifyDelta_(orphanId, cornerBounds, deltaPos);
    var desiredBounds = {
      left: cornerBounds.left + deltaPos.x,
      top: cornerBounds.top + deltaPos.y,
      width: bounds.width,
      height: bounds.height
    };

    this.updateOffsetAndPosition_(desiredBounds, layoutPosition, layout);
  },

  /**
   * @param {string} parentId
   * @param {boolean} recurse Include descendants of children.
   * @return {!Array<string>}
   * @private
   */
  findChildren_: function(parentId, recurse) {
    var children = [];
    for (let childId of this.displayLayoutMap_.keys()) {
      if (childId == parentId)
        continue;
      if (this.displayLayoutMap_.get(childId).parentId == parentId) {
        // Insert immediate children at the front of the array.
        children.unshift(childId);
        if (recurse) {
          // Descendants get added to the end of the list.
          children = children.concat(this.findChildren_(childId, true));
        }
      }
    }
    return children;
  },

  /**
   * Recursively calculates the absolute bounds of a display.
   * Caches the display bounds so that parent bounds are only calculated once.
   * @param {string} id
   * @param {number} width
   * @param {number} height
   * @private
   */
  calculateBounds_: function(id, width, height) {
    var left, top;
    var layout = this.displayLayoutMap_.get(id);
    if (this.mirroring || !layout || !layout.parentId) {
      left = -width / 2;
      top = -height / 2;
    } else {
      if (!this.calculatedBoundsMap_.has(layout.parentId)) {
        var pbounds = this.displayBoundsMap_.get(layout.parentId);
        this.calculateBounds_(layout.parentId, pbounds.width, pbounds.height);
      }
      var parentBounds = this.getCalculatedDisplayBounds(layout.parentId);
      left = parentBounds.left;
      top = parentBounds.top;
      switch (layout.position) {
        case chrome.system.display.LayoutPosition.TOP:
          left += layout.offset;
          top -= height;
          break;
        case chrome.system.display.LayoutPosition.RIGHT:
          left += parentBounds.width;
          top += layout.offset;
          break;
        case chrome.system.display.LayoutPosition.BOTTOM:
          left += layout.offset;
          top += parentBounds.height;
          break;
        case chrome.system.display.LayoutPosition.LEFT:
          left -= width;
          top += layout.offset;
          break;
      }
    }
    var result = {
      left: left,
      top: top,
      width: width,
      height: height,
    };
    this.setCalculatedDisplayBounds_(id, result);
  },

  /**
   * Finds the display closest to |bounds| ignoring |opt_ignoreIds|.
   * @param {string} displayId
   * @param {!chrome.system.display.Bounds} bounds
   * @param {Array<string>=} opt_ignoreIds Ids to ignore.
   * @return {string}
   * @private
   */
  findClosest_: function(displayId, bounds, opt_ignoreIds) {
    var x = bounds.left + bounds.width / 2;
    var y = bounds.top + bounds.height / 2;
    var closestId = '';
    var closestDelta2 = 0;
    for (let otherId of this.calculatedBoundsMap_.keys()) {
      if (otherId == displayId)
        continue;
      if (opt_ignoreIds && opt_ignoreIds.includes(otherId))
        continue;
      var otherBounds = this.getCalculatedDisplayBounds(otherId);
      var left = otherBounds.left;
      var top = otherBounds.top;
      var width = otherBounds.width;
      var height = otherBounds.height;
      if (x >= left && x < left + width && y >= top && y < top + height)
        return otherId;  // point is inside rect
      var dx, dy;
      if (x < left)
        dx = left - x;
      else if (x > left + width)
        dx = x - (left + width);
      else
        dx = 0;
      if (y < top)
        dy = top - y;
      else if (y > top + height)
        dy = y - (top + height);
      else
        dy = 0;
      var delta2 = dx * dx + dy * dy;
      if (closestId == '' || delta2 < closestDelta2) {
        closestId = otherId;
        closestDelta2 = delta2;
      }
    }
    return closestId;
  },

  /**
   * Calculates the LayoutPosition for |bounds| relative to |parentId|.
   * @param {!chrome.system.display.Bounds} bounds
   * @param {!chrome.system.display.Bounds} parentBounds
   * @return {!chrome.system.display.LayoutPosition}
   */
  getLayoutPositionForBounds_: function(bounds, parentBounds) {
    // Translate bounds from top-left to center.
    var x = bounds.left + bounds.width / 2;
    var y = bounds.top + bounds.height / 2;

    // Determine the distance from the new bounds to both of the near edges.
    var left = parentBounds.left;
    var top = parentBounds.top;
    var width = parentBounds.width;
    var height = parentBounds.height;

    // Signed deltas to the center.
    var dx = x - (left + width / 2);
    var dy = y - (top + height / 2);

    // Unsigned distance to each edge.
    var distx = Math.abs(dx) - width / 2;
    var disty = Math.abs(dy) - height / 2;

    if (distx > disty) {
      if (dx < 0)
        return chrome.system.display.LayoutPosition.LEFT;
      else
        return chrome.system.display.LayoutPosition.RIGHT;
    } else {
      if (dy < 0)
        return chrome.system.display.LayoutPosition.TOP;
      else
        return chrome.system.display.LayoutPosition.BOTTOM;
    }
  },

  /**
   * Modifies |bounds| to the position closest to it along the edge of
   * |parentId| specified by |layoutPosition|.
   * @param {!chrome.system.display.Bounds} bounds
   * @param {string} parentId
   * @param {!chrome.system.display.LayoutPosition} layoutPosition
   * @return {!{x: number, y: number}}
   */
  snapBounds_: function(bounds, parentId, layoutPosition) {
    var parentBounds = this.getCalculatedDisplayBounds(parentId);

    var x;
    if (layoutPosition == chrome.system.display.LayoutPosition.LEFT) {
      x = parentBounds.left - bounds.width;
    } else if (layoutPosition == chrome.system.display.LayoutPosition.RIGHT) {
      x = parentBounds.left + parentBounds.width;
    } else {
      x = this.snapToX_(bounds, parentBounds);
    }

    var y;
    if (layoutPosition == chrome.system.display.LayoutPosition.TOP) {
      y = parentBounds.top - bounds.height;
    } else if (layoutPosition == chrome.system.display.LayoutPosition.BOTTOM) {
      y = parentBounds.top + parentBounds.height;
    } else {
      y = this.snapToY_(bounds, parentBounds);
    }

    return {x: x, y: y};
  },

  /**
   * Snaps a horizontal value, see snapToEdge.
   * @param {!chrome.system.display.Bounds} newBounds
   * @param {!chrome.system.display.Bounds} parentBounds
   * @param {number=} opt_snapDistance Provide to override the snap distance.
   *     0 means snap from any distance.
   * @return {number}
   */
  snapToX_: function(newBounds, parentBounds, opt_snapDistance) {
    return this.snapToEdge_(
        newBounds.left, newBounds.width, parentBounds.left, parentBounds.width,
        opt_snapDistance);
  },

  /**
   * Snaps a vertical value, see snapToEdge.
   * @param {!chrome.system.display.Bounds} newBounds
   * @param {!chrome.system.display.Bounds} parentBounds
   * @param {number=} opt_snapDistance Provide to override the snap distance.
   *     0 means snap from any distance.
   * @return {number}
   */
  snapToY_: function(newBounds, parentBounds, opt_snapDistance) {
    return this.snapToEdge_(
        newBounds.top, newBounds.height, parentBounds.top, parentBounds.height,
        opt_snapDistance);
  },

  /**
   * Snaps the region [point, width] to [basePoint, baseWidth] if
   * the [point, width] is close enough to the base's edge.
   * @param {number} point The starting point of the region.
   * @param {number} width The width of the region.
   * @param {number} basePoint The starting point of the base region.
   * @param {number} baseWidth The width of the base region.
   * @param {number=} opt_snapDistance Provide to override the snap distance.
   *     0 means snap at any distance.
   * @return {number} The moved point. Returns the point itself if it doesn't
   *     need to snap to the edge.
   * @private
   */
  snapToEdge_: function(point, width, basePoint, baseWidth, opt_snapDistance) {
    // If the edge of the region is smaller than this, it will snap to the
    // base's edge.
    /** @const */ var SNAP_DISTANCE_PX = 16;
    var snapDist =
        (opt_snapDistance !== undefined) ? opt_snapDistance : SNAP_DISTANCE_PX;

    var startDiff = Math.abs(point - basePoint);
    var endDiff = Math.abs(point + width - (basePoint + baseWidth));
    // Prefer the closer one if both edges are close enough.
    if ((!snapDist || startDiff < snapDist) && startDiff < endDiff)
      return basePoint;
    else if (!snapDist || endDiff < snapDist)
      return basePoint + baseWidth - width;

    return point;
  },

  /**
   * Intersects |layout| with each other layout and reduces |deltaPos| to
   * avoid any collisions (or sets it to [0,0] if the display can not be moved
   * in the direction of |deltaPos|).
   * Note: this assumes that deltaPos is already 'snapped' to the parent edge,
   * and therefore will not collide with the parent, i.e. this is to prevent
   * overlapping with displays other than the parent.
   * @param {string} id
   * @param {!chrome.system.display.Bounds} bounds
   * @param {!{x: number, y: number}} deltaPos
   */
  collideAndModifyDelta_: function(id, bounds, deltaPos) {
    var keys = this.calculatedBoundsMap_.keys();
    var others = new Set(keys);
    others.delete(id);
    var checkCollisions = true;
    while (checkCollisions) {
      checkCollisions = false;
      for (let otherId of others) {
        var otherBounds = this.getCalculatedDisplayBounds(otherId);
        if (this.collideWithBoundsAndModifyDelta_(
                bounds, otherBounds, deltaPos)) {
          if (deltaPos.x == 0 && deltaPos.y == 0)
            return;
          others.delete(otherId);
          checkCollisions = true;
          break;
        }
      }
    }
  },

  /**
   * Intersects |bounds| with |otherBounds|. If there is a collision, modifies
   * |deltaPos| to limit movement to a single axis and avoid the collision
   * and returns true. See note for |collideAndModifyDelta_|.
   * @param {!chrome.system.display.Bounds} bounds
   * @param {!chrome.system.display.Bounds} otherBounds
   * @param {!{x: number, y: number}} deltaPos
   * @return {boolean} Whether there was a collision.
   */
  collideWithBoundsAndModifyDelta_: function(bounds, otherBounds, deltaPos) {
    var newX = bounds.left + deltaPos.x;
    var newY = bounds.top + deltaPos.y;

    if ((newX + bounds.width <= otherBounds.left) ||
        (newX >= otherBounds.left + otherBounds.width) ||
        (newY + bounds.height <= otherBounds.top) ||
        (newY >= otherBounds.top + otherBounds.height)) {
      return false;
    }

    // |deltaPos| should already be restricted to X or Y. This shortens the
    // delta to stay outside the bounds, however it does not change the sign of
    // the delta, i.e. it does not "push" the point outside the bounds if
    // the point is already inside.
    if (Math.abs(deltaPos.x) > Math.abs(deltaPos.y)) {
      deltaPos.y = 0;
      let snapDeltaX;
      if (deltaPos.x > 0) {
        let x = otherBounds.left - bounds.width;
        snapDeltaX = Math.max(0, x - bounds.left);
      } else {
        let x = otherBounds.left + otherBounds.width;
        snapDeltaX = Math.min(x - bounds.left, 0);
      }
      deltaPos.x = snapDeltaX;
    } else {
      deltaPos.x = 0;
      let snapDeltaY;
      if (deltaPos.y > 0) {
        let y = otherBounds.top - bounds.height;
        snapDeltaY = Math.min(0, y - bounds.top);
      } else if (deltaPos.y < 0) {
        let y = otherBounds.top + otherBounds.height;
        snapDeltaY = Math.max(y - bounds.top, 0);
      } else {
        snapDeltaY = 0;
      }
      deltaPos.y = snapDeltaY;
    }

    return true;
  },

  /**
   * Updates the offset for |layout| from |bounds|.
   * @param {!chrome.system.display.Bounds} bounds
   * @param {!chrome.system.display.LayoutPosition} position
   * @param {!chrome.system.display.DisplayLayout} layout
   */
  updateOffsetAndPosition_: function(bounds, position, layout) {
    layout.position = position;
    if (!layout.parentId) {
      layout.offset = 0;
      return;
    }

    // Offset is calculated from top or left edge.
    var parentBounds = this.getCalculatedDisplayBounds(layout.parentId);
    var offset, minOffset, maxOffset;
    if (position == chrome.system.display.LayoutPosition.LEFT ||
        position == chrome.system.display.LayoutPosition.RIGHT) {
      offset = bounds.top - parentBounds.top;
      minOffset = -bounds.height;
      maxOffset = parentBounds.height;
    } else {
      offset = bounds.left - parentBounds.left;
      minOffset = -bounds.width;
      maxOffset = parentBounds.width;
    }
    /** @const */ var MIN_OFFSET_OVERLAP = 50;
    minOffset += MIN_OFFSET_OVERLAP;
    maxOffset -= MIN_OFFSET_OVERLAP;
    layout.offset = Math.max(minOffset, Math.min(offset, maxOffset));

    // Update the calculated bounds to match the new offset.
    this.calculateBounds_(layout.id, bounds.width, bounds.height);
  },

  /**
   * Returns |bounds| translated to touch the closest corner of |parentBounds|.
   * @param {!chrome.system.display.Bounds} bounds
   * @param {!chrome.system.display.Bounds} parentBounds
   * @return {!chrome.system.display.Bounds}
   * @private
   */
  getCornerBounds_: function(bounds, parentBounds) {
    var x;
    if (bounds.left > parentBounds.left + parentBounds.width / 2)
      x = parentBounds.left + parentBounds.width;
    else
      x = parentBounds.left - bounds.width;
    var y;
    if (bounds.top > parentBounds.top + parentBounds.height / 2)
      y = parentBounds.top + parentBounds.height;
    else
      y = parentBounds.top - bounds.height;
    return {
      left: x,
      top: y,
      width: bounds.width,
      height: bounds.height,
    };
  },

  /**
   * Highlights the edge of the div associated with |id| based on
   * |layoutPosition| and removes any other highlights. If |layoutPosition| is
   * undefined, removes all highlights.
   * @param {string} id
   * @param {chrome.system.display.LayoutPosition|undefined} layoutPosition
   * @private
   */
  highlightEdge_: function(id, layoutPosition) {
    for (let layout of this.layouts) {
      var highlight = (layout.id == id) ? layoutPosition : undefined;
      var div = this.$$('#_' + layout.id);
      div.classList.toggle(
          'highlight-right',
          highlight == chrome.system.display.LayoutPosition.RIGHT);
      div.classList.toggle(
          'highlight-left',
          highlight == chrome.system.display.LayoutPosition.LEFT);
      div.classList.toggle(
          'highlight-top',
          highlight == chrome.system.display.LayoutPosition.TOP);
      div.classList.toggle(
          'highlight-bottom',
          highlight == chrome.system.display.LayoutPosition.BOTTOM);
    }
  },
};
