// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function(exports) {
  /**
   * Orientation of a line.
   * @enum {boolean}
   */
  var Orientation = {
    VERTICAL: false,
    HORIZONTAL: true
  }

  /**
   * Map from keysetId to layout.
   * @type {Map<String,KeysetLayout>}
   * @private
   */
  var layouts = {};

  /**
   * Container for storing a keyset's layout.
   */
  var KeysetLayout = function() {
    this.keys = [];
  }

  KeysetLayout.prototype = {
    /**
     * All keys in the keyset.
     * @type {Array<Key>}
     */
    keys: undefined,

    /**
     * Spacial partitioning of all keys in the keyset.
     * @type {DecisionNode}
     */
    tree: undefined,

    /**
     * Add a key to the keyset.
     */
    add: function(key) {
      this.keys.push(key);
    },

    /**
     * Regenerates a decision tree using the keys in the keyset.
     */
    regenerateTree: function() {
      // Split using horizontal lines first, as keyboards tend to be
      // row-centric.
      var splits = findSplits(this.keys, Orientation.HORIZONTAL);
      this.tree = createBinaryTree(0, splits.length - 1, splits);
      if (this.tree)
        this.tree.populate(this.keys);
    },

    /**
     * Searches the tree for the key closest to the point provided.
     * @param {number} x The x-coordinate.
     * @param {number} y The y-coordinate.
     * @return {?kb-key} The key, or null if none found.
     */
    findClosestKey: function(x, y) {
      var closestNode = this.tree.findClosestNode(x, y);
      var key = closestNode.data;
      if (!key)
        return;
      // Ignore touches that aren't close.
      return key.distanceTo(x,y) <= MAX_TOUCH_FUZZ_DISTANCE ?
          key.key : null;
    },

    /**
     * Returns the position data of all keys in this keyset.
     * @return {Array<Map>}
     */
    getLayout: function() {
      return this.keys.map(function(key) {
        return key.toMap();
      });
    },
  };

  /**
   * Container for caching a key's data.
   * @param {Object} key The key to cache.
   * @param {number} left The x-coordinate of the left edge of the key.
   * @param {number} top The y-coordinate of the top edge of the key.
   * @param {number} width The width of the key in px.
   * @param {number} height The height of the key in px.
   */
  var Key = function(key) {
    this.key = key;
    var style = key.style;
    this.top = parseFloat(style.top) - KEY_PADDING_TOP;
    this.left = parseFloat(style.left);
    this.right = this.left + parseFloat(style.width);
    this.bottom = this.top + parseFloat(style.height) + KEY_PADDING_TOP
        + KEY_PADDING_BOTTOM;
  }

  Key.prototype = {
    /**
     * Manhattan distance from the the provided point to the key.
     * @param {number} x The x-coordinate of the point.
     * @param {number} y The y-coordinate of the point.
     * @return {number}.
     */
    distanceTo: function (x, y) {
      return Math.abs(this.intersect(new Line(x))) +
          Math.abs(this.intersect(new Line(y, true)));
    },

    /**
     * Checks whether the key intersects with the line provided.
     * @param {!Line} line The line.
     * @return {number} Zero if it intersects, signed manhattan distance if it
     *     does not.
     */
    intersect: function(line) {
      var min = line.rotated ? this.top : this.left;
      var max = line.rotated ? this.bottom : this.right;
      return (line.c > max) ? line.c - max : Math.min(0, line.c - min);
    },

    /**
     * Returns the Key as a map.
     * @return {Map<String,number>}
     */
    toMap: function() {
      return {
        'x': this.left,
        'y': this.top,
        'width': this.right - this.left,
        'height': this.bottom - this.bottom,
      }
    },
  };

  /**
   * Object representing the line y = c or x = c.
   * @param {number} The x or y coordinate of the intersection line depending on
   *    on orientation.
   * @param {Orientation} orientation The orientation of the line.
   */
  var Line = function(c, orientation) {
    this.c = c;
    this.rotated = orientation;
  };

  Line.prototype = {
    /**
     * The position of the provided point in relation to the line.
     * @param {number} x The x-coordinate of the point.
     * @param {number} y The y-coordinate of the point.
     * @return {number} Zero if they intersect, negative if the point is before
     *    the line, positive if it's after.
     */
    testPoint: function (x, y) {
      var c = this.rotated ? y : x;
      return this.c == c ? 0 : c - this.c;
    },

    test: function (key) {
      // Key already provides an intersect method. If the key is to the right of
      // the line, then the line is to the left of the key.
      return -1 * key.intersect(this);
    },
  };

  /**
   * A node used to split 2D space.
   * @param {Line} The line to split the space with.
   */
  var DecisionNode = function(line) {
    this.decision = line;
  };

  DecisionNode.prototype = {
    /**
     * The test whether to proceed in the left or right branch.
     * @type {Line}
     */
    decision: undefined,

    /**
     * The branch for nodes that failed the decision test.
     * @type {?DecisionNode}
     */
    fail: undefined,

    /**
     * The branch for nodes that passed the decision test.
     * @type {?DecisionNode}
     */
    pass: undefined,

    /**
     * Finds the node closest to the point provided.
     * @param {number} x The x-coordinate.
     * @param {number} y The y-coordinate.
     * @return {DecisionNode | LeafNode}
     */
    findClosestNode: function(x, y) {
      return this.search(function(node){
        return node.decision.testPoint(x,y) >= 0;
      });
    },

    /**
     * Populates the decision tree with elements.
     * @param {Array{Key}} The child elements.
     */
    populate: function(data) {
      if (!data.length)
        return;
      var pass = [];
      var fail = [];
      for (var i = 0; i< data.length; i++) {
        var result = this.decision.test(data[i]);
        // Add to both branches if result == 0.
        if (result >= 0)
          pass.push(data[i]);
        if (result <= 0)
          fail.push(data[i]);
      }
      var currentRotation = this.decision.rotated;
      /**
       * Splits the tree further such that each leaf has exactly one data point.
       * @param {Array} array The data points.
       * @return {DecisionNode | LeafNode} The new branch for the tree.
       */
      var updateBranch = function(array) {
        if (array.length == 1) {
          return new LeafNode(array[0]);
        } else {
          var splits = findSplits(array, !currentRotation)
          var tree = createBinaryTree(0, splits.length - 1, splits);
          tree.populate(array);
          return tree;
        }
      };
      // All elements that passed the decision test.
      if (pass.length > 0) {
        if (this.pass)
          this.pass.populate(pass);
        else
          this.pass = updateBranch(pass);
      }
      // All elements that failed the decision test.
      if (fail.length > 0) {
        if (this.fail)
          this.fail.populate(fail);
        else
          this.fail = updateBranch(fail);
      }
    },

    /**
     * Searches for the first leaf that matches the search function.
     * @param {Function<DecisionNode>: Boolean} searchFn The function used to
     *    determine whether to search in the left or right subtree.
     * @param {DecisionNode | LeafNode} The node that most closely matches the
     *    search parameters.
     */
    search: function(searchFn) {
      if (searchFn(this)) {
        return this.pass ? this.pass.search(searchFn) : this;
      }
      return this.fail ? this.fail.search(searchFn) : this;
    },

    /**
     * Tests whether the key belongs in the left or right branch of this node.
     * @param {Key} key The key being tested.
     * @return {boolean} Whether it belongs in the right branch.
     */
    test: function(key) {
      return this.decision.testKey(key)
    },
  };

  /**
   * Structure representing a leaf in the decision tree. It contains a single
   * data point.
   */
  var LeafNode = function(data) {
    this.data = data;
  };

  LeafNode.prototype = {
    search: function() {
      return this;
    },
  };

  /**
   * Converts the array to a binary tree.
   * @param {number} start The start index.
   * @param {number} end The end index.
   * @param {Array} nodes The array to convert.
   * @return {DecisionNode}
   */
  var createBinaryTree = function(start, end, nodes) {
    if (start > end)
      return;
    var midpoint = Math.floor((end + start)/2);
    var root = new DecisionNode(nodes[midpoint]);
    root.fail = createBinaryTree(start, midpoint - 1, nodes);
    root.pass = createBinaryTree(midpoint + 1, end, nodes);
    return root;
  };

  /**
   * Calculates the optimum split points on the specified axis.
   * @param {Array<Keys>} allKeys All keys in the keyset.
   * @param {Partition=} axis Whether to split on the y-axis instead.
   * @return {Array<Line>} The optimum split points.
   */
  var findSplits = function(allKeys, orientation) {
    /**
     * Returns the minimum edge on the key.
     * @param {Key} The key.
     * @return {number}
     */
    var getMin = function(key) {
      return orientation == Orientation.HORIZONTAL ? key.top : key.left;
    };

    /**
     * Returns the maximum edge on the key.
     * @param {Key} The key.
     */
    var getMax = function(key) {
      return orientation == Orientation.HORIZONTAL ? key.bottom : key.right;
    };

    /**
     * Returns a duplicate free version of array.
     * @param {Array} array A sorted array.
     * @return {Array} Sorted array without duplicates.
     */
    var unique = function(array) {
      var result = [];
      for (var i = 0; i< array.length; i++) {
        if (i == 0 || result[result.length -1] != array[i])
            result.push(array[i]);
      }
      return result;
    };

    /**
     * Creates an array of zeroes.
     * @param {number} length The length of the array.
     * @return {Array{number}}
     */
    var zeroes = function(length) {
      var array = new Array(length);
      for (var i = 0; i < length; i++) {
        array[i] = 0;
      }
      return array;
    }
    // All edges of keys.
    var edges = [];
    for (var i = 0; i < allKeys.length; i++) {
      var key = allKeys[i];
      var min = getMin(key);
      var max = getMax(key);
      edges.push(min);
      edges.push(max);
    }
    // Array.sort() sorts lexicographically by default.
    edges.sort(function(a, b) {
      return a - b;
    });
    edges = unique(edges);
    // Container for projection sum from edge i to edge i + 1.
    var intervalWeight = zeroes(edges.length);

    for (var i = 0; i < allKeys.length; i++) {
      var key = allKeys[i];
      var min = getMin(key);
      var max = getMax(key);
      var index =
          exports.binarySearch(edges, 0, edges.length - 1, function(index) {
        var edge = edges[index];
        return edge == min ? 0 : min - edge;
      });
      if (index < 0 || min != edges[index]) {
        console.error("Unable to split keys.");
        return;
      }
      // Key can span multiple edges.
      for (var j = index; j < edges.length && edges[j] < max; j++) {
        intervalWeight[j] ++;
      }
    }

    var splits = [];
    // Min and max are bad splits.
    for (var i = 1; i < intervalWeight.length - 1; i++) {
      if (intervalWeight[i] < intervalWeight[i - 1]) {
        var mid = Math.abs((edges[i] + edges[i+1]) / 2)
        splits.push(new Line(mid, orientation));
      }
    }
    return splits;
  }

  /**
   * Caches the layout of current keysets.
   */
  function recordKeysets_() {
    layouts = {};
    var keysets = $('keyboard').querySelectorAll('kb-keyset').array();
    for (var i = 0; i < keysets.length; i++) {
      var keyset = keysets[i];
      var layout = new KeysetLayout();
      var rows = keyset.querySelectorAll('kb-row').array();
      for (var j = 0; j < rows.length; j++) {
        var row = rows[j];
        var nodes = row.children;
        for (var k = 0 ; k < nodes.length; k++) {
          layout.add(new Key(nodes[k]));
        }
      }
      layout.regenerateTree();
      layouts[keyset.id] = layout;
    }
  };

  /**
   * Returns the layout of the keyset.
   * @param{!String} id The id of the keyset.
   * @private
   */
  var getKeysetLayout_ = function(id) {
    return layouts[id];
  };

  exports.getKeysetLayout = getKeysetLayout_;
  exports.recordKeysets = recordKeysets_;
})(this);
