// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  /**
   * This class represents a collection of non-intersecting ranges. Ranges
   * specified by (start, end) can be added and removed at will. It is used to
   * record which sections of a media file have been cached, e.g. the first and
   * last few kB plus several MB in the middle.
   *
   * Example usage:
   * someRange.add(0, 100);     // Contains 0-100.
   * someRange.add(150, 200);   // Contains 0-100, 150-200.
   * someRange.remove(25, 75);  // Contains 0-24, 76-100, 150-200.
   * someRange.add(25, 149);    // Contains 0-200.
   */
  function DisjointRangeSet() {
    this.ranges_ = {};
  }

  DisjointRangeSet.prototype = {
    /**
     * Deletes all ranges intersecting with (start ... end) and returns the
     * extents of the cleared area.
     * @param {int} start The start of the range to remove.
     * @param {int} end The end of the range to remove.
     * @param {int} sloppiness 0 removes only strictly overlapping ranges, and
     *                         1 removes adjacent ones.
     * @return {Object} The start and end of the newly cleared range.
     */
    clearRange: function(start, end, sloppiness) {
      var ranges = this.ranges_;
      var result = {start: start, end: end};

      for (var rangeStart in this.ranges_) {
        rangeEnd = this.ranges_[rangeStart];
        // A range intersects another if its start lies within the other range
        // or vice versa.
        if ((rangeStart >= start && rangeStart <= (end + sloppiness)) ||
            (start >= rangeStart && start <= (rangeEnd + sloppiness))) {
          delete ranges[rangeStart];
          result.start = Math.min(result.start, rangeStart);
          result.end = Math.max(result.end, rangeEnd);
        }
      }

      return result;
    },

    /**
     * Adds a range to this DisjointRangeSet.
     * Joins adjacent and overlapping ranges together.
     * @param {int} start The beginning of the range to add, inclusive.
     * @param {int} end The end of the range to add, inclusive.
     */
    add: function(start, end) {
      if (end < start)
        return;

      // Remove all touching ranges.
      result = this.clearRange(start, end, 1);
      // Add back a single contiguous range.
      this.ranges_[Math.min(start, result.start)] = Math.max(end, result.end);
    },

    /**
     * Combines a DisjointRangeSet with this one.
     * @param {DisjointRangeSet} ranges A DisjointRangeSet to be squished into
     * this one.
     */
    merge: function(other) {
      var ranges = this;
      other.forEach(function(start, end) { ranges.add(start, end); });
    },

    /**
     * Removes a range from this DisjointRangeSet.
     * Will split existing ranges if necessary.
     * @param {int} start The beginning of the range to remove, inclusive.
     * @param {int} end The end of the range to remove, inclusive.
     */
    remove: function(start, end) {
      if (end < start)
        return;

      // Remove instersecting ranges.
      result = this.clearRange(start, end, 0);

      // Add back non-overlapping ranges.
      if (result.start < start)
        this.ranges_[result.start] = start - 1;
      if (result.end > end)
        this.ranges_[end + 1] = result.end;
    },

    /**
     * Iterates over every contiguous range in this DisjointRangeSet, calling a
     * function for each (start, end).
     * @param {function(int, int)} iterator The function to call on each range.
     */
    forEach: function(iterator) {
      for (var start in this.ranges_)
        iterator(start, this.ranges_[start]);
    },

    /**
     * Maps this DisjointRangeSet to an array by calling a given function on the
     * start and end of each contiguous range, sorted by start.
     * @param {function(int, int)} mapper Maps a range to an array element.
     * @return {Array} An array of each mapper(range).
     */
    map: function(mapper) {
      var starts = [];
      for (var start in this.ranges_)
        starts.push(parseInt(start));
      starts.sort(function(a, b) {
        return a - b;
      });

      var ranges = this.ranges_;
      var results = starts.map(function(s) {
        return mapper(s, ranges[s]);
      });

      return results;
    },

    /**
     * Finds the maximum value present in any of the contained ranges.
     * @return {int} The maximum value contained by this DisjointRangeSet.
     */
    max: function() {
      var max = -Infinity;
      for (var start in this.ranges_)
        max = Math.max(max, this.ranges_[start]);
      return max;
    },
  };

  return {
    DisjointRangeSet: DisjointRangeSet
  };
});
