// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * This class stores the filter queries as history and highlight the log text
 * to increase the readability of the log
 *
 *   - Enable / Disable highlights
 *   - Highlights text with multiple colors
 *   - Resolve hghlight conficts (A text highlighted by multiple colors) so that
 *     the latest added highlight always has highest priority to display.
 *
 */
var CrosLogMarker = (function() {
  'use strict';

  // Special classes (defined in log_visualizer_view.css)
  var LOG_MARKER_HIGHLIGHT_CLASS = 'cros-log-visualizer-marker-highlight';
  var LOG_MARKER_CONTAINER_ID = 'cros-log-visualizer-marker-container';
  var LOG_MARKER_HISTORY_ENTRY_CLASS =
      'cros-log-visualizer-marker-history-entry';
  var LOG_MARKER_HISTORY_COLOR_TAG_CLASS =
          'cros-log-visualizer-marker-history-color-tag';

  /**
   * Colors used for highlighting. (Current we support 6 colors)
   * TODO(shinfan): Add more supoorted colors.
   */
  var COLOR_USAGE_SET = {
    'Crimson': false,
    'DeepSkyBlue': false,
    'DarkSeaGreen': false,
    'GoldenRod': false,
    'IndianRed': false,
    'Orange': false
  };
  var COLOR_NUMBER = Object.keys(COLOR_USAGE_SET).length;


  /**
   * CrosHighlightTag represents a single highlight tag in text.
   */
  var CrosHighlightTag = (function() {
    /**
     * @constructor
     */
    function CrosHighlightTag(color, field, range, priority) {
      this.color = color;
      this.field = field;
      this.range = range;
      this.priority = priority;
      this.enabled = true;
    }

    return CrosHighlightTag;
  })();

  /**
   * @constructor
   * @param {CrosLogVisualizerView} logVisualizerView A reference to
   *     CrosLogVisualizerView.
   */
  function CrosLogMarker(logVisualizerView) {
    this.container = $(LOG_MARKER_CONTAINER_ID);
    // Stores highlight objects for each entry.
    this.entryHighlights = [];
    // Stores all the filter queries.
    this.markHistory = {};
    // Object references from CrosLogVisualizerView.
    this.logEntries = logVisualizerView.logEntries;
    this.logVisualizerView = logVisualizerView;
    // Counts how many highlights are created.
    this.markCount = 0;
    for (var i = 0; i < this.logEntries.length; i++) {
      this.entryHighlights.push([]);
    }
  }

  CrosLogMarker.prototype = {
    /**
     * Saves the query to the mark history and highlights the text
     * based on the query.
     */
    addMarkHistory: function(query) {
      // Increases the counter
      this.markCount += 1;

      // Find an avaiable color.
      var color = this.pickColor();
      if (!color) {
        // If all colors are occupied.
        alert('You can only add at most ' + COLOR_NUMBER + 'markers.');
        return;
      }

      // Updates HTML elements.
      var historyEntry = addNode(this.container, 'div');
      historyEntry.className = LOG_MARKER_HISTORY_ENTRY_CLASS;

      // A color tag that indicats the color used.
      var colorTag = addNode(historyEntry, 'div');
      colorTag.className = LOG_MARKER_HISTORY_COLOR_TAG_CLASS;
      colorTag.style.background = color;

      // Displays the query text.
      var queryText = addNodeWithText(historyEntry, 'p', query);
      queryText.style.color = color;

      // Adds a button to remove the marker.
      var removeBtn = addNodeWithText(historyEntry, 'a', 'Remove');
      removeBtn.addEventListener(
          'click', this.onRemoveBtnClicked_.bind(this, historyEntry, color));

      // A checkbox that lets user enable and disable the marker.
      var enableCheckbox = addNode(historyEntry, 'input');
      enableCheckbox.type = 'checkbox';
      enableCheckbox.checked = true;
      enableCheckbox.color = color;
      enableCheckbox.addEventListener('change',
          this.onEnableCheckboxChange_.bind(this, enableCheckbox), false);

      // Searches log text for matched patterns and highlights them.
      this.patternMatch(query, color);
    },

    /**
     * Search the text for matched strings
     */
    patternMatch: function(query, color) {
      var pattern = new RegExp(query, 'i');
      for (var i = 0; i < this.logEntries.length; i++) {
        var entry = this.logEntries[i];
        // Search description of each log entry
        // TODO(shinfan): Add more search fields
        var positions = this.findPositions(
            pattern, entry.description);
        for (var j = 0; j < positions.length; j++) {
            var pos = positions[j];
            this.mark(entry, pos, 'description', color);
        }
        this.sortHighlightsByStartPosition_(this.entryHighlights[i]);
      }
    },

    /**
     * Highlights the text.
     * @param {CrosLogEntry} entry The log entry to be highlighted
     * @param {int|Array} position [start, end]
     * @param {string} field The field of entry to be highlighted
     * @param {string} color color used for highlighting
     */
    mark: function(entry, position, field, color) {
      // Creates the highlight object
      var tag = new CrosHighlightTag(color, field, position, this.markCount);
      // Add the highlight into entryHighlights
      this.entryHighlights[entry.rowNum].push(tag);
    },

    /**
     * Find the highlight objects that covers the given position
     * @param {CrosHighlightTag|Array} highlights highlights of a log entry
     * @param {int} position The target index
     * @param {string} field The target field
     * @return {CrosHighlightTag|Array} Highlights that cover the position
     */
    getHighlight: function(highlights, index, field) {
      var res = [];
      for (var j = 0; j < highlights.length; j++) {
        var highlight = highlights[j];
        if (highlight.range[0] <= index &&
            highlight.range[1] > index &&
            highlight.field == field &&
            highlight.enabled) {
          res.push(highlight);
        }
      }
      /**
       * Sorts the result by priority so that the highlight with
       * highest priority comes first.
       */
      this.sortHighlightsByPriority_(res);
      return res;
    },

    /**
     * This function highlights the entry by going through the text from left
     * to right and searching for "key" positions.
     * A "key" position is a position that one (or more) highlight
     * starts or ends. We only care about "key" positions because this is where
     * the text highlight status changes.
     * At each key position, the function decides if the text between this
     * position and previous position need to be highlighted and resolves
     * highlight conflicts.
     *
     * @param {CrosLogEntry} entry The entry going to be highlighted.
     * @param {string} field The specified field of the entry.
     * @param {DOMElement} parent Parent node.
     */
    getHighlightedEntry: function(entry, field, parent) {
      var rowNum = entry.rowNum;
      // Get the original text content of the entry (without any highlights).
      var content = this.logEntries[rowNum][field];
      var index = 0;
      while (index < content.length) {
        var nextIndex = this.getNextIndex(
            this.entryHighlights[rowNum], index, field, content);
        // Searches for highlights that have the position in range.
        var highlights = this.getHighlight(
            this.entryHighlights[rowNum], index, field);
        var text = content.substr(index, nextIndex - index);
        if (highlights.length > 0) {
          // Always picks the highlight with highest priority.
          this.addSpan(text, highlights[0].color, parent);
        } else {
          addNodeWithText(parent, 'span', text);
        }
        index = nextIndex;
      }
    },

    /**
     * A helper function that is used by this.getHightlightedEntry
     * It returns the first index where a highlight begins or ends from
     * the given index.
     * @param {CrosHighlightTag|Array} highlights An array of highlights
     *     of a log entry.
     * @param {int} index The start position.
     * @param {string} field The specified field of entry.
     *     Other fields are ignored.
     * @param {string} content The text content of the log entry.
     * @return {int} The first index where a highlight begins or ends.
     */
    getNextIndex: function(highlights, index, field, content) {
      var minGap = Infinity;
      var res = -1;
      for (var i = 0; i < highlights.length; i++) {
        if (highlights[i].field != field || !highlights[i].enabled)
          continue;
        // Distance between current index and the start index of highlight.
        var gap1 = highlights[i].range[0] - index;
        // Distance between current index and the end index of highlight.
        var gap2 = highlights[i].range[1] - index;
        if (gap1 > 0 && gap1 < minGap) {
          minGap = gap1;
          res = highlights[i].range[0];
        }
        if (gap2 > 0 && gap2 < minGap) {
          minGap = gap2;
          res = highlights[i].range[1];
        }
      }
      // Returns |res| if found. Otherwise returns the end position of the text.
      return res > 0 ? res : content.length;
    },

    /**
     * A helper function that is used by this.getHightlightedEntry.
     * It adds the HTML label to the text.
     */
    addSpan: function(text, color, parent) {
      var span = addNodeWithText(parent, 'span', text);
      span.style.color = color;
      span.className = LOG_MARKER_HIGHLIGHT_CLASS;
    },

    /**
     * A helper function that is used by this.getHightlightedEntry.
     * It adds the HTML label to the text.
     */
    pickColor: function() {
      for (var color in COLOR_USAGE_SET) {
        if (!COLOR_USAGE_SET[color]) {
          COLOR_USAGE_SET[color] = true;
          return color;
        }
      }
      return false;
    },

    /**
     * A event handler that enables and disables the corresponding marker.
     * @private
     */
    onEnableCheckboxChange_: function(checkbox) {
      for (var i = 0; i < this.entryHighlights.length; i++) {
        for (var j = 0; j < this.entryHighlights[i].length; j++) {
          if (this.entryHighlights[i][j].color == checkbox.color) {
            this.entryHighlights[i][j].enabled = checkbox.checked;
           }
        }
      }
      this.refreshLogTable();
    },

    /**
     * A event handlier that removes the marker from history.
     * @private
     */
    onRemoveBtnClicked_: function(entry, color) {
      entry.parentNode.removeChild(entry);
      COLOR_USAGE_SET[color] = false;
      for (var i = 0; i < this.entryHighlights.length; i++) {
        var highlights = this.entryHighlights[i];
        while (true) {
          var index = this.findHighlightByColor_(highlights, color);
          if (index == -1)
            break;
          highlights.splice(index, 1);
        }
      }
      this.refreshLogTable();
    },

    /**
     * A helper function that returns the index of first highlight that
     * has the target color. Otherwise returns -1.
     * @private
     */
    findHighlightByColor_: function(highlights, color) {
      for (var i = 0; i < highlights.length; i++) {
        if (highlights[i].color == color)
          return i;
      }
      return -1;
    },

    /**
     * Refresh the log table in the CrosLogVisualizerView.
     */
    refreshLogTable: function() {
      this.logVisualizerView.populateTable();
      this.logVisualizerView.filterLog();
    },

    /**
     * A pattern can appear multiple times in a string.
     * Returns positions of all the appearance.
     */
    findPositions: function(pattern, str) {
      var res = [];
      str = str.toLowerCase();
      var match = str.match(pattern);
      if (!match)
        return res;
      for (var i = 0; i < match.length; i++) {
        var index = 0;
        while (true) {
          var start = str.indexOf(match[i].toLowerCase(), index);
          if (start == -1)
            break;
          var end = start + match[i].length;
          res.push([start, end]);
          index = end + 1;
        }
      }
      return res;
    },

    /**
     * A helper function used in sorting highlights by start position.
     * @param {HighlightTag} h1, h2 Two highlight tags in the array.
     * @private
     */
    compareStartPosition_: function(h1, h2) {
      return h1.range[0] - h2.range[0];
    },

    /**
     * A helper function used in sorting highlights by priority.
     * @param {HighlightTag} h1, h2 Two highlight tags in the array.
     * @private
     */
    comparePriority_: function(h1, h2) {
      return h2.priority - h1.priority;
    },

    /**
     * A helper function that sorts the highlights array by start position.
     * @private
     */
    sortHighlightsByStartPosition_: function(highlights) {
      highlights.sort(this.compareStartPosition_);
    },

    /**
     * A helper function that sorts the highlights array by priority.
     * @private
     */
    sortHighlightsByPriority_: function(highlights) {
      highlights.sort(this.comparePriority_);
    }
  };

  return CrosLogMarker;
})();
