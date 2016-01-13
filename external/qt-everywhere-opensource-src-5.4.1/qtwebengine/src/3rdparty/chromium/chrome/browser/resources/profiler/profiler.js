// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_browserBridge;
var g_mainView;

// TODO(eroman): The handling of "max" across snapshots is not correct.
// For starters the browser needs to be aware to generate new maximums.
// Secondly, we need to take into account the "max" of intermediary snapshots,
// not just the terminal ones.

/**
 * Main entry point called once the page has loaded.
 */
function onLoad() {
  g_browserBridge = new BrowserBridge();
  g_mainView = new MainView();
}

document.addEventListener('DOMContentLoaded', onLoad);

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser. Used as a singleton.
 */
var BrowserBridge = (function() {
  'use strict';

  /**
   * @constructor
   */
  function BrowserBridge() {
  }

  BrowserBridge.prototype = {
    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    sendGetData: function() {
      chrome.send('getData');
    },

    sendResetData: function() {
      chrome.send('resetData');
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receivedData: function(data) {
      // TODO(eroman): The browser should give an indication of which snapshot
      // this data belongs to. For now we always assume it is for the latest.
      g_mainView.addDataToSnapshot(data);
    },
  };

  return BrowserBridge;
})();

/**
 * This class handles the presentation of our profiler view. Used as a
 * singleton.
 */
var MainView = (function() {
  'use strict';

  // --------------------------------------------------------------------------
  // Important IDs in the HTML document
  // --------------------------------------------------------------------------

  // The search box to filter results.
  var FILTER_SEARCH_ID = 'filter-search';

  // The container node to put all the "Group by" dropdowns into.
  var GROUP_BY_CONTAINER_ID = 'group-by-container';

  // The container node to put all the "Sort by" dropdowns into.
  var SORT_BY_CONTAINER_ID = 'sort-by-container';

  // The DIV to put all the tables into.
  var RESULTS_DIV_ID = 'results-div';

  // The container node to put all the column (visibility) checkboxes into.
  var COLUMN_TOGGLES_CONTAINER_ID = 'column-toggles-container';

  // The container node to put all the column (merge) checkboxes into.
  var COLUMN_MERGE_TOGGLES_CONTAINER_ID = 'column-merge-toggles-container';

  // The anchor which toggles visibility of column checkboxes.
  var EDIT_COLUMNS_LINK_ID = 'edit-columns-link';

  // The container node to show/hide when toggling the column checkboxes.
  var EDIT_COLUMNS_ROW = 'edit-columns-row';

  // The checkbox which controls whether things like "Worker Threads" and
  // "PAC threads" will be merged together.
  var MERGE_SIMILAR_THREADS_CHECKBOX_ID = 'merge-similar-threads-checkbox';

  var RESET_DATA_LINK_ID = 'reset-data-link';

  var TOGGLE_SNAPSHOTS_LINK_ID = 'snapshots-link';
  var SNAPSHOTS_ROW = 'snapshots-row';
  var SNAPSHOT_SELECTION_SUMMARY_ID = 'snapshot-selection-summary';
  var TAKE_SNAPSHOT_BUTTON_ID = 'take-snapshot-button';

  var SAVE_SNAPSHOTS_BUTTON_ID = 'save-snapshots-button';
  var SNAPSHOT_FILE_LOADER_ID = 'snapshot-file-loader';
  var LOAD_ERROR_ID = 'file-load-error';

  var DOWNLOAD_ANCHOR_ID = 'download-anchor';

  // --------------------------------------------------------------------------
  // Row keys
  // --------------------------------------------------------------------------

  // Each row of our data is an array of values rather than a dictionary. This
  // avoids some overhead from repeating the key string multiple times, and
  // speeds up the property accesses a bit. The following keys are well-known
  // indexes into the array for various properties.
  //
  // Note that the declaration order will also define the default display order.

  var BEGIN_KEY = 1;  // Start at 1 rather than 0 to simplify sorting code.
  var END_KEY = BEGIN_KEY;

  var KEY_COUNT = END_KEY++;
  var KEY_RUN_TIME = END_KEY++;
  var KEY_AVG_RUN_TIME = END_KEY++;
  var KEY_MAX_RUN_TIME = END_KEY++;
  var KEY_QUEUE_TIME = END_KEY++;
  var KEY_AVG_QUEUE_TIME = END_KEY++;
  var KEY_MAX_QUEUE_TIME = END_KEY++;
  var KEY_BIRTH_THREAD = END_KEY++;
  var KEY_DEATH_THREAD = END_KEY++;
  var KEY_PROCESS_TYPE = END_KEY++;
  var KEY_PROCESS_ID = END_KEY++;
  var KEY_FUNCTION_NAME = END_KEY++;
  var KEY_SOURCE_LOCATION = END_KEY++;
  var KEY_FILE_NAME = END_KEY++;
  var KEY_LINE_NUMBER = END_KEY++;

  var NUM_KEYS = END_KEY - BEGIN_KEY;

  // --------------------------------------------------------------------------
  // Aggregators
  // --------------------------------------------------------------------------

  // To generalize computing/displaying the aggregate "counts" for each column,
  // we specify an optional "Aggregator" class to use with each property.

  // The following are actually "Aggregator factories". They create an
  // aggregator instance by calling 'create()'. The instance is then fed
  // each row one at a time via the 'consume()' method. After all rows have
  // been consumed, the 'getValueAsText()' method will return the aggregated
  // value.

  /**
   * This aggregator counts the number of unique values that were fed to it.
   */
  var UniquifyAggregator = (function() {
    function Aggregator(key) {
      this.key_ = key;
      this.valuesSet_ = {};
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.valuesSet_[e[this.key_]] = true;
      },

      getValueAsText: function() {
        return getDictionaryKeys(this.valuesSet_).length + ' unique';
      },
    };

    return {
      create: function(key) { return new Aggregator(key); }
    };
  })();

  /**
   * This aggregator sums a numeric field.
   */
  var SumAggregator = (function() {
    function Aggregator(key) {
      this.key_ = key;
      this.sum_ = 0;
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.sum_ += e[this.key_];
      },

      getValue: function() {
        return this.sum_;
      },

      getValueAsText: function() {
        return formatNumberAsText(this.getValue());
      },
    };

    return {
      create: function(key) { return new Aggregator(key); }
    };
  })();

  /**
   * This aggregator computes an average by summing two
   * numeric fields, and then dividing the totals.
   */
  var AvgAggregator = (function() {
    function Aggregator(numeratorKey, divisorKey) {
      this.numeratorKey_ = numeratorKey;
      this.divisorKey_ = divisorKey;

      this.numeratorSum_ = 0;
      this.divisorSum_ = 0;
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.numeratorSum_ += e[this.numeratorKey_];
        this.divisorSum_ += e[this.divisorKey_];
      },

      getValue: function() {
        return this.numeratorSum_ / this.divisorSum_;
      },

      getValueAsText: function() {
        return formatNumberAsText(this.getValue());
      },
    };

    return {
      create: function(numeratorKey, divisorKey) {
        return {
          create: function(key) {
            return new Aggregator(numeratorKey, divisorKey);
          },
        };
      }
    };
  })();

  /**
   * This aggregator finds the maximum for a numeric field.
   */
  var MaxAggregator = (function() {
    function Aggregator(key) {
      this.key_ = key;
      this.max_ = -Infinity;
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.max_ = Math.max(this.max_, e[this.key_]);
      },

      getValue: function() {
        return this.max_;
      },

      getValueAsText: function() {
        return formatNumberAsText(this.getValue());
      },
    };

    return {
      create: function(key) { return new Aggregator(key); }
    };
  })();

  // --------------------------------------------------------------------------
  // Key properties
  // --------------------------------------------------------------------------

  // Custom comparator for thread names (sorts main thread and IO thread
  // higher than would happen lexicographically.)
  var threadNameComparator =
      createLexicographicComparatorWithExceptions([
          'CrBrowserMain',
          'Chrome_IOThread',
          'Chrome_FileThread',
          'Chrome_HistoryThread',
          'Chrome_DBThread',
          'Still_Alive',
      ]);

  function diffFuncForCount(a, b) {
    return b - a;
  }

  function diffFuncForMax(a, b) {
    return b;
  }

  /**
   * Enumerates information about various keys. Such as whether their data is
   * expected to be numeric or is a string, a descriptive name (title) for the
   * property, and what function should be used to aggregate the property when
   * displayed in a column.
   *
   * --------------------------------------
   * The following properties are required:
   * --------------------------------------
   *
   *   [name]: This is displayed as the column's label.
   *   [aggregator]: Aggregator factory that is used to compute an aggregate
   *                 value for this column.
   *
   * --------------------------------------
   * The following properties are optional:
   * --------------------------------------
   *
   *   [inputJsonKey]: The corresponding key for this property in the original
   *                   JSON dictionary received from the browser. If this is
   *                   present, values for this key will be automatically
   *                   populated during import.
   *   [comparator]: A comparator function for sorting this column.
   *   [textPrinter]: A function that transforms values into the user-displayed
   *                  text shown in the UI. If unspecified, will default to the
   *                  "toString()" function.
   *   [cellAlignment]: The horizonal alignment to use for columns of this
   *                    property (for instance 'right'). If unspecified will
   *                    default to left alignment.
   *   [sortDescending]: When first clicking on this column, we will default to
   *                     sorting by |comparator| in ascending order. If this
   *                     property is true, we will reverse that to descending.
   *   [diff]: Function to call to compute a "difference" value between
   *           parameters (a, b). This is used when calculating the difference
   *           between two snapshots. Diffing numeric quantities generally
   *           involves subtracting, but some fields like max may need to do
   *           something different.
   */
  var KEY_PROPERTIES = [];

  KEY_PROPERTIES[KEY_PROCESS_ID] = {
    name: 'PID',
    cellAlignment: 'right',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_PROCESS_TYPE] = {
    name: 'Process type',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_BIRTH_THREAD] = {
    name: 'Birth thread',
    inputJsonKey: 'birth_thread',
    aggregator: UniquifyAggregator,
    comparator: threadNameComparator,
  };

  KEY_PROPERTIES[KEY_DEATH_THREAD] = {
    name: 'Exec thread',
    inputJsonKey: 'death_thread',
    aggregator: UniquifyAggregator,
    comparator: threadNameComparator,
  };

  KEY_PROPERTIES[KEY_FUNCTION_NAME] = {
    name: 'Function name',
    inputJsonKey: 'birth_location.function_name',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_FILE_NAME] = {
    name: 'File name',
    inputJsonKey: 'birth_location.file_name',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_LINE_NUMBER] = {
    name: 'Line number',
    cellAlignment: 'right',
    inputJsonKey: 'birth_location.line_number',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_COUNT] = {
    name: 'Count',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    inputJsonKey: 'death_data.count',
    aggregator: SumAggregator,
    diff: diffFuncForCount,
  };

  KEY_PROPERTIES[KEY_QUEUE_TIME] = {
    name: 'Total queue time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    inputJsonKey: 'death_data.queue_ms',
    aggregator: SumAggregator,
    diff: diffFuncForCount,
  };

  KEY_PROPERTIES[KEY_MAX_QUEUE_TIME] = {
    name: 'Max queue time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    inputJsonKey: 'death_data.queue_ms_max',
    aggregator: MaxAggregator,
    diff: diffFuncForMax,
  };

  KEY_PROPERTIES[KEY_RUN_TIME] = {
    name: 'Total run time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    inputJsonKey: 'death_data.run_ms',
    aggregator: SumAggregator,
    diff: diffFuncForCount,
  };

  KEY_PROPERTIES[KEY_AVG_RUN_TIME] = {
    name: 'Avg run time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    aggregator: AvgAggregator.create(KEY_RUN_TIME, KEY_COUNT),
  };

  KEY_PROPERTIES[KEY_MAX_RUN_TIME] = {
    name: 'Max run time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    inputJsonKey: 'death_data.run_ms_max',
    aggregator: MaxAggregator,
    diff: diffFuncForMax,
  };

  KEY_PROPERTIES[KEY_AVG_QUEUE_TIME] = {
    name: 'Avg queue time',
    cellAlignment: 'right',
    sortDescending: true,
    textPrinter: formatNumberAsText,
    aggregator: AvgAggregator.create(KEY_QUEUE_TIME, KEY_COUNT),
  };

  KEY_PROPERTIES[KEY_SOURCE_LOCATION] = {
    name: 'Source location',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  /**
   * Returns the string name for |key|.
   */
  function getNameForKey(key) {
    var props = KEY_PROPERTIES[key];
    if (props == undefined)
      throw 'Did not define properties for key: ' + key;
    return props.name;
  }

  /**
   * Ordered list of all keys. This is the order we generally want
   * to display the properties in. Default to declaration order.
   */
  var ALL_KEYS = [];
  for (var k = BEGIN_KEY; k < END_KEY; ++k)
    ALL_KEYS.push(k);

  // --------------------------------------------------------------------------
  // Default settings
  // --------------------------------------------------------------------------

  /**
   * List of keys for those properties which we want to initially omit
   * from the table. (They can be re-enabled by clicking [Edit columns]).
   */
  var INITIALLY_HIDDEN_KEYS = [
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
    KEY_QUEUE_TIME,
  ];

  /**
   * The ordered list of grouping choices to expose in the "Group by"
   * dropdowns. We don't include the numeric properties, since they
   * leads to awkward bucketing.
   */
  var GROUPING_DROPDOWN_CHOICES = [
    KEY_PROCESS_TYPE,
    KEY_PROCESS_ID,
    KEY_BIRTH_THREAD,
    KEY_DEATH_THREAD,
    KEY_FUNCTION_NAME,
    KEY_SOURCE_LOCATION,
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
  ];

  /**
   * The ordered list of sorting choices to expose in the "Sort by"
   * dropdowns.
   */
  var SORT_DROPDOWN_CHOICES = ALL_KEYS;

  /**
   * The ordered list of all columns that can be displayed in the tables (not
   * including whatever has been hidden via [Edit Columns]).
   */
  var ALL_TABLE_COLUMNS = ALL_KEYS;

  /**
   * The initial keys to sort by when loading the page (can be changed later).
   */
  var INITIAL_SORT_KEYS = [-KEY_COUNT];

  /**
   * The default sort keys to use when nothing has been specified.
   */
  var DEFAULT_SORT_KEYS = [-KEY_COUNT];

  /**
   * The initial keys to group by when loading the page (can be changed later).
   */
  var INITIAL_GROUP_KEYS = [];

  /**
   * The columns to give the option to merge on.
   */
  var MERGEABLE_KEYS = [
    KEY_PROCESS_ID,
    KEY_PROCESS_TYPE,
    KEY_BIRTH_THREAD,
    KEY_DEATH_THREAD,
  ];

  /**
   * The columns to merge by default.
   */
  var INITIALLY_MERGED_KEYS = [];

  /**
   * The full set of columns which define the "identity" for a row. A row is
   * considered equivalent to another row if it matches on all of these
   * fields. This list is used when merging the data, to determine which rows
   * should be merged together. The remaining columns not listed in
   * IDENTITY_KEYS will be aggregated.
   */
  var IDENTITY_KEYS = [
    KEY_BIRTH_THREAD,
    KEY_DEATH_THREAD,
    KEY_PROCESS_TYPE,
    KEY_PROCESS_ID,
    KEY_FUNCTION_NAME,
    KEY_SOURCE_LOCATION,
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
  ];

  /**
   * The time (in milliseconds) to wait after receiving new data before
   * re-drawing it to the screen. The reason we wait a bit is to avoid
   * repainting repeatedly during the loading phase (which can slow things
   * down). Note that this only slows down the addition of new data. It does
   * not impact the  latency of user-initiated operations like sorting or
   * merging.
   */
  var PROCESS_DATA_DELAY_MS = 500;

  /**
   * The initial number of rows to display (the rest are hidden) when no
   * grouping is selected. We use a higher limit than when grouping is used
   * since there is a lot of vertical real estate.
   */
  var INITIAL_UNGROUPED_ROW_LIMIT = 30;

  /**
   * The initial number of rows to display (rest are hidden) for each group.
   */
  var INITIAL_GROUP_ROW_LIMIT = 10;

  /**
   * The number of extra rows to show/hide when clicking the "Show more" or
   * "Show less" buttons.
   */
  var LIMIT_INCREMENT = 10;

  // --------------------------------------------------------------------------
  // General utility functions
  // --------------------------------------------------------------------------

  /**
   * Returns a list of all the keys in |dict|.
   */
  function getDictionaryKeys(dict) {
    var keys = [];
    for (var key in dict) {
      keys.push(key);
    }
    return keys;
  }

  /**
   * Formats the number |x| as a decimal integer. Strips off any decimal parts,
   * and comma separates the number every 3 characters.
   */
  function formatNumberAsText(x) {
    var orig = x.toFixed(0);

    var parts = [];
    for (var end = orig.length; end > 0; ) {
      var chunk = Math.min(end, 3);
      parts.push(orig.substr(end - chunk, chunk));
      end -= chunk;
    }
    return parts.reverse().join(',');
  }

  /**
   * Simple comparator function which works for both strings and numbers.
   */
  function simpleCompare(a, b) {
    if (a == b)
      return 0;
    if (a < b)
      return -1;
    return 1;
  }

  /**
   * Returns a comparator function that compares values lexicographically,
   * but special-cases the values in |orderedList| to have a higher
   * rank.
   */
  function createLexicographicComparatorWithExceptions(orderedList) {
    var valueToRankMap = {};
    for (var i = 0; i < orderedList.length; ++i)
      valueToRankMap[orderedList[i]] = i;

    function getCustomRank(x) {
      var rank = valueToRankMap[x];
      if (rank == undefined)
        rank = Infinity;  // Unmatched.
      return rank;
    }

    return function(a, b) {
      var aRank = getCustomRank(a);
      var bRank = getCustomRank(b);

      // Not matched by any of our exceptions.
      if (aRank == bRank)
        return simpleCompare(a, b);

      if (aRank < bRank)
        return -1;
      return 1;
    };
  }

  /**
   * Returns dict[key]. Note that if |key| contains periods (.), they will be
   * interpreted as meaning a sub-property.
   */
  function getPropertyByPath(dict, key) {
    var cur = dict;
    var parts = key.split('.');
    for (var i = 0; i < parts.length; ++i) {
      if (cur == undefined)
        return undefined;
      cur = cur[parts[i]];
    }
    return cur;
  }

  /**
   * Creates and appends a DOM node of type |tagName| to |parent|. Optionally,
   * sets the new node's text to |opt_text|. Returns the newly created node.
   */
  function addNode(parent, tagName, opt_text) {
    var n = parent.ownerDocument.createElement(tagName);
    parent.appendChild(n);
    if (opt_text != undefined) {
      addText(n, opt_text);
    }
    return n;
  }

  /**
   * Adds |text| to |parent|.
   */
  function addText(parent, text) {
    var textNode = parent.ownerDocument.createTextNode(text);
    parent.appendChild(textNode);
    return textNode;
  }

  /**
   * Deletes all the strings in |array| which appear in |valuesToDelete|.
   */
  function deleteValuesFromArray(array, valuesToDelete) {
    var valueSet = arrayToSet(valuesToDelete);
    for (var i = 0; i < array.length; ) {
      if (valueSet[array[i]]) {
        array.splice(i, 1);
      } else {
        i++;
      }
    }
  }

  /**
   * Deletes all the repeated ocurrences of strings in |array|.
   */
  function deleteDuplicateStringsFromArray(array) {
    // Build up set of each entry in array.
    var seenSoFar = {};

    for (var i = 0; i < array.length; ) {
      var value = array[i];
      if (seenSoFar[value]) {
        array.splice(i, 1);
      } else {
        seenSoFar[value] = true;
        i++;
      }
    }
  }

  /**
   * Builds a map out of the array |list|.
   */
  function arrayToSet(list) {
    var set = {};
    for (var i = 0; i < list.length; ++i)
      set[list[i]] = true;
    return set;
  }

  function trimWhitespace(text) {
    var m = /^\s*(.*)\s*$/.exec(text);
    return m[1];
  }

  /**
   * Selects the option in |select| which has a value of |value|.
   */
  function setSelectedOptionByValue(select, value) {
    for (var i = 0; i < select.options.length; ++i) {
      if (select.options[i].value == value) {
        select.options[i].selected = true;
        return true;
      }
    }
    return false;
  }

  /**
   * Adds a checkbox to |parent|. The checkbox will have a label on its right
   * with text |label|. Returns the checkbox input node.
   */
  function addLabeledCheckbox(parent, label) {
    var labelNode = addNode(parent, 'label');
    var checkbox = addNode(labelNode, 'input');
    checkbox.type = 'checkbox';
    addText(labelNode, label);
    return checkbox;
  }

  /**
   * Return the last component in a path which is separated by either forward
   * slashes or backslashes.
   */
  function getFilenameFromPath(path) {
    var lastSlash = Math.max(path.lastIndexOf('/'),
                             path.lastIndexOf('\\'));
    if (lastSlash == -1)
      return path;

    return path.substr(lastSlash + 1);
  }

  /**
   * Returns the current time in milliseconds since unix epoch.
   */
  function getTimeMillis() {
    return (new Date()).getTime();
  }

  /**
   * Toggle a node between hidden/invisible.
   */
  function toggleNodeDisplay(n) {
    if (n.style.display == '') {
      n.style.display = 'none';
    } else {
      n.style.display = '';
    }
  }

  /**
   * Set the visibility state of a node.
   */
  function setNodeDisplay(n, visible) {
    if (visible) {
      n.style.display = '';
    } else {
      n.style.display = 'none';
    }
  }

  // --------------------------------------------------------------------------
  // Functions that augment, bucket, and compute aggregates for the input data.
  // --------------------------------------------------------------------------

  /**
   * Adds new derived properties to row. Mutates the provided dictionary |e|.
   */
  function augmentDataRow(e) {
    computeDataRowAverages(e);
    e[KEY_SOURCE_LOCATION] = e[KEY_FILE_NAME] + ' [' + e[KEY_LINE_NUMBER] + ']';
  }

  function computeDataRowAverages(e) {
    e[KEY_AVG_QUEUE_TIME] = e[KEY_QUEUE_TIME] / e[KEY_COUNT];
    e[KEY_AVG_RUN_TIME] = e[KEY_RUN_TIME] / e[KEY_COUNT];
  }

  /**
   * Creates and initializes an aggregator object for each key in |columns|.
   * Returns an array whose keys are values from |columns|, and whose
   * values are Aggregator instances.
   */
  function initializeAggregates(columns) {
    var aggregates = [];

    for (var i = 0; i < columns.length; ++i) {
      var key = columns[i];
      var aggregatorFactory = KEY_PROPERTIES[key].aggregator;
      aggregates[key] = aggregatorFactory.create(key);
    }

    return aggregates;
  }

  function consumeAggregates(aggregates, row) {
    for (var key in aggregates)
      aggregates[key].consume(row);
  }

  function bucketIdenticalRows(rows, identityKeys, propertyGetterFunc) {
    var identicalRows = {};
    for (var i = 0; i < rows.length; ++i) {
      var r = rows[i];

      var rowIdentity = [];
      for (var j = 0; j < identityKeys.length; ++j)
        rowIdentity.push(propertyGetterFunc(r, identityKeys[j]));
      rowIdentity = rowIdentity.join('\n');

      var l = identicalRows[rowIdentity];
      if (!l) {
        l = [];
        identicalRows[rowIdentity] = l;
      }
      l.push(r);
    }
    return identicalRows;
  }

  /**
   * Merges the rows in |origRows|, by collapsing the columns listed in
   * |mergeKeys|. Returns an array with the merged rows (in no particular
   * order).
   *
   * If |mergeSimilarThreads| is true, then threads with a similar name will be
   * considered equivalent. For instance, "WorkerThread-1" and "WorkerThread-2"
   * will be remapped to "WorkerThread-*".
   *
   * If |outputAsDictionary| is false then the merged rows will be returned as a
   * flat list. Otherwise the result will be a dictionary, where each row
   * has a unique key.
   */
  function mergeRows(origRows, mergeKeys, mergeSimilarThreads,
                     outputAsDictionary) {
    // Define a translation function for each property. Normally we copy over
    // properties as-is, but if we have been asked to "merge similar threads" we
    // we will remap the thread names that end in a numeric suffix.
    var propertyGetterFunc;

    if (mergeSimilarThreads) {
      propertyGetterFunc = function(row, key) {
        var value = row[key];
        // If the property is a thread name, try to remap it.
        if (key == KEY_BIRTH_THREAD || key == KEY_DEATH_THREAD) {
          var m = /^(.*[^\d])(\d+)$/.exec(value);
          if (m)
            value = m[1] + '*';
        }
        return value;
      }
    } else {
      propertyGetterFunc = function(row, key) { return row[key]; };
    }

    // Determine which sets of properties a row needs to match on to be
    // considered identical to another row.
    var identityKeys = IDENTITY_KEYS.slice(0);
    deleteValuesFromArray(identityKeys, mergeKeys);

    // Set |aggregateKeys| to everything else, since we will be aggregating
    // their value as part of the merge.
    var aggregateKeys = ALL_KEYS.slice(0);
    deleteValuesFromArray(aggregateKeys, IDENTITY_KEYS);
    deleteValuesFromArray(aggregateKeys, mergeKeys);

    // Group all the identical rows together, bucketed into |identicalRows|.
    var identicalRows =
        bucketIdenticalRows(origRows, identityKeys, propertyGetterFunc);

    var mergedRows = outputAsDictionary ? {} : [];

    // Merge the rows and save the results to |mergedRows|.
    for (var k in identicalRows) {
      // We need to smash the list |l| down to a single row...
      var l = identicalRows[k];

      var newRow = [];

      if (outputAsDictionary) {
        mergedRows[k] = newRow;
      } else {
        mergedRows.push(newRow);
      }

      // Copy over all the identity columns to the new row (since they
      // were the same for each row matched).
      for (var i = 0; i < identityKeys.length; ++i)
        newRow[identityKeys[i]] = propertyGetterFunc(l[0], identityKeys[i]);

      // Compute aggregates for the other columns.
      var aggregates = initializeAggregates(aggregateKeys);

      // Feed the rows to the aggregators.
      for (var i = 0; i < l.length; ++i)
        consumeAggregates(aggregates, l[i]);

      // Suck out the data generated by the aggregators.
      for (var aggregateKey in aggregates)
        newRow[aggregateKey] = aggregates[aggregateKey].getValue();
    }

    return mergedRows;
  }

  /**
   * Takes two dictionaries data1 and data2, and returns a new flat list which
   * represents the difference between them. The exact meaning of "difference"
   * is column specific, but for most numeric fields (like the count, or total
   * time), it is found by subtracting.
   *
   * Rows in data1 and data2 are expected to use the same scheme for the keys.
   * In other words, data1[k] is considered the analagous row to data2[k].
   */
  function subtractSnapshots(data1, data2, columnsToExclude) {
    // These columns are computed from the other columns. We won't bother
    // diffing/aggregating these, but rather will derive them again from the
    // final row.
    var COMPUTED_AGGREGATE_KEYS = [KEY_AVG_QUEUE_TIME, KEY_AVG_RUN_TIME];

    // These are the keys which determine row equality. Since we are not doing
    // any merging yet at this point, it is simply the list of all identity
    // columns.
    var identityKeys = IDENTITY_KEYS.slice(0);
    deleteValuesFromArray(identityKeys, columnsToExclude);

    // The columns to compute via aggregation is everything else.
    var aggregateKeys = ALL_KEYS.slice(0);
    deleteValuesFromArray(aggregateKeys, IDENTITY_KEYS);
    deleteValuesFromArray(aggregateKeys, COMPUTED_AGGREGATE_KEYS);
    deleteValuesFromArray(aggregateKeys, columnsToExclude);

    var diffedRows = [];

    for (var rowId in data2) {
      var row1 = data1[rowId];
      var row2 = data2[rowId];

      var newRow = [];

      // Copy over all the identity columns to the new row (since they
      // were the same for each row matched).
      for (var i = 0; i < identityKeys.length; ++i)
        newRow[identityKeys[i]] = row2[identityKeys[i]];

      // Diff the two rows.
      if (row1) {
        for (var i = 0; i < aggregateKeys.length; ++i) {
          var aggregateKey = aggregateKeys[i];
          var a = row1[aggregateKey];
          var b = row2[aggregateKey];

          var diffFunc = KEY_PROPERTIES[aggregateKey].diff;
          newRow[aggregateKey] = diffFunc(a, b);
        }
      } else {
        // If the the row doesn't appear in snapshot1, then there is nothing to
        // diff, so just copy row2 as is.
        for (var i = 0; i < aggregateKeys.length; ++i) {
          var aggregateKey = aggregateKeys[i];
          newRow[aggregateKey] = row2[aggregateKey];
        }
      }

      if (newRow[KEY_COUNT] == 0) {
        // If a row's count has gone to zero, it means there were no new
        // occurrences of it in the second snapshot, so remove it.
        continue;
      }

      // Since we excluded the averages during the diffing phase, re-compute
      // them using the diffed totals.
      computeDataRowAverages(newRow);
      diffedRows.push(newRow);
    }

    return diffedRows;
  }

  // --------------------------------------------------------------------------
  // HTML drawing code
  // --------------------------------------------------------------------------

  function getTextValueForProperty(key, value) {
    if (value == undefined) {
      // A value may be undefined as a result of having merging rows. We
      // won't actually draw it, but this might be called by the filter.
      return '';
    }

    var textPrinter = KEY_PROPERTIES[key].textPrinter;
    if (textPrinter)
      return textPrinter(value);
    return value.toString();
  }

  /**
   * Renders the property value |value| into cell |td|. The name of this
   * property is |key|.
   */
  function drawValueToCell(td, key, value) {
    // Get a text representation of the value.
    var text = getTextValueForProperty(key, value);

    // Apply the desired cell alignment.
    var cellAlignment = KEY_PROPERTIES[key].cellAlignment;
    if (cellAlignment)
      td.align = cellAlignment;

    if (key == KEY_SOURCE_LOCATION) {
      // Linkify the source column so it jumps to the source code. This doesn't
      // take into account the particular code this build was compiled from, or
      // local edits to source. It should however work correctly for top of tree
      // builds.
      var m = /^(.*) \[(\d+)\]$/.exec(text);
      if (m) {
        var filepath = m[1];
        var filename = getFilenameFromPath(filepath);
        var linenumber = m[2];

        var link = addNode(td, 'a', filename + ' [' + linenumber + ']');
        // http://chromesrc.appspot.com is a server I wrote specifically for
        // this task. It redirects to the appropriate source file; the file
        // paths given by the compiler can be pretty crazy and different
        // between platforms.
        link.href = 'http://chromesrc.appspot.com/?path=' +
                    encodeURIComponent(filepath) + '&line=' + linenumber;
        link.target = '_blank';
        return;
      }
    }

    // String values can get pretty long. If the string contains no spaces, then
    // CSS fails to wrap it, and it overflows the cell causing the table to get
    // really big. We solve this using a hack: insert a <wbr> element after
    // every single character. This will allow the rendering engine to wrap the
    // value, and hence avoid it overflowing!
    var kMinLengthBeforeWrap = 20;

    addText(td, text.substr(0, kMinLengthBeforeWrap));
    for (var i = kMinLengthBeforeWrap; i < text.length; ++i) {
      addNode(td, 'wbr');
      addText(td, text.substr(i, 1));
    }
  }

  // --------------------------------------------------------------------------
  // Helper code for handling the sort and grouping dropdowns.
  // --------------------------------------------------------------------------

  function addOptionsForGroupingSelect(select) {
    // Add "no group" choice.
    addNode(select, 'option', '---').value = '';

    for (var i = 0; i < GROUPING_DROPDOWN_CHOICES.length; ++i) {
      var key = GROUPING_DROPDOWN_CHOICES[i];
      var option = addNode(select, 'option', getNameForKey(key));
      option.value = key;
    }
  }

  function addOptionsForSortingSelect(select) {
    // Add "no sort" choice.
    addNode(select, 'option', '---').value = '';

    // Add a divider.
    addNode(select, 'optgroup').label = '';

    for (var i = 0; i < SORT_DROPDOWN_CHOICES.length; ++i) {
      var key = SORT_DROPDOWN_CHOICES[i];
      addNode(select, 'option', getNameForKey(key)).value = key;
    }

    // Add a divider.
    addNode(select, 'optgroup').label = '';

    // Add the same options, but for descending.
    for (var i = 0; i < SORT_DROPDOWN_CHOICES.length; ++i) {
      var key = SORT_DROPDOWN_CHOICES[i];
      var n = addNode(select, 'option', getNameForKey(key) + ' (DESC)');
      n.value = reverseSortKey(key);
    }
  }

  /**
   * Helper function used to update the sorting and grouping lists after a
   * dropdown changes.
   */
  function updateKeyListFromDropdown(list, i, select) {
    // Update the list.
    if (i < list.length) {
      list[i] = select.value;
    } else {
      list.push(select.value);
    }

    // Normalize the list, so setting 'none' as primary zeros out everything
    // else.
    for (var i = 0; i < list.length; ++i) {
      if (list[i] == '') {
        list.splice(i, list.length - i);
        break;
      }
    }
  }

  /**
   * Comparator for property |key|, having values |value1| and |value2|.
   * If the key has defined a custom comparator use it. Otherwise use a
   * default "less than" comparison.
   */
  function compareValuesForKey(key, value1, value2) {
    var comparator = KEY_PROPERTIES[key].comparator;
    if (comparator)
      return comparator(value1, value2);
    return simpleCompare(value1, value2);
  }

  function reverseSortKey(key) {
    return -key;
  }

  function sortKeyIsReversed(key) {
    return key < 0;
  }

  function sortKeysMatch(key1, key2) {
    return Math.abs(key1) == Math.abs(key2);
  }

  function getKeysForCheckedBoxes(checkboxes) {
    var keys = [];
    for (var k in checkboxes) {
      if (checkboxes[k].checked)
        keys.push(k);
    }
    return keys;
  }

  // --------------------------------------------------------------------------

  /**
   * @constructor
   */
  function MainView() {
    // Make sure we have a definition for each key.
    for (var k = BEGIN_KEY; k < END_KEY; ++k) {
      if (!KEY_PROPERTIES[k])
        throw 'KEY_PROPERTIES[] not defined for key: ' + k;
    }

    this.init_();
  }

  MainView.prototype = {
    addDataToSnapshot: function(data) {
      // TODO(eroman): We need to know which snapshot this data belongs to!
      // For now we assume it is the most recent snapshot.
      var snapshotIndex = this.snapshots_.length - 1;

      var snapshot = this.snapshots_[snapshotIndex];

      var pid = data.process_id;
      var ptype = data.process_type;

      // Save the browser's representation of the data
      snapshot.origData.push(data);

      // Augment each data row with the process information.
      var rows = data.list;
      for (var i = 0; i < rows.length; ++i) {
        // Transform the data from a dictionary to an array. This internal
        // representation is more compact and faster to access.
        var origRow = rows[i];
        var newRow = [];

        newRow[KEY_PROCESS_ID] = pid;
        newRow[KEY_PROCESS_TYPE] = ptype;

        // Copy over the known properties which have a 1:1 mapping with JSON.
        for (var k = BEGIN_KEY; k < END_KEY; ++k) {
          var inputJsonKey = KEY_PROPERTIES[k].inputJsonKey;
          if (inputJsonKey != undefined) {
            newRow[k] = getPropertyByPath(origRow, inputJsonKey);
          }
        }

        if (newRow[KEY_COUNT] == 0) {
          // When resetting the data, it is possible for the backend to give us
          // counts of "0". There is no point adding these rows (in fact they
          // will cause us to do divide by zeros when calculating averages and
          // stuff), so we skip past them.
          continue;
        }

        // Add our computed properties.
        augmentDataRow(newRow);

        snapshot.flatData.push(newRow);
      }

      if (!arrayToSet(this.getSelectedSnapshotIndexes_())[snapshotIndex]) {
        // Optimization: If this snapshot is not a data dependency for the
        // current display, then don't bother updating anything.
        return;
      }

      // We may end up calling addDataToSnapshot_() repeatedly (once for each
      // process). To avoid this from slowing us down we do bulk updates on a
      // timer.
      this.updateMergedDataSoon_();
    },

    updateMergedDataSoon_: function() {
      if (this.updateMergedDataPending_) {
        // If a delayed task has already been posted to re-merge the data,
        // then we don't need to do anything extra.
        return;
      }

      // Otherwise schedule updateMergedData_() to be called later. We want it
      // to be called no more than once every PROCESS_DATA_DELAY_MS
      // milliseconds.

      if (this.lastUpdateMergedDataTime_ == undefined)
        this.lastUpdateMergedDataTime_ = 0;

      var timeSinceLastMerge = getTimeMillis() - this.lastUpdateMergedDataTime_;
      var timeToWait = Math.max(0, PROCESS_DATA_DELAY_MS - timeSinceLastMerge);

      var functionToRun = function() {
        // Do the actual update.
        this.updateMergedData_();
        // Keep track of when we last ran.
        this.lastUpdateMergedDataTime_ = getTimeMillis();
        this.updateMergedDataPending_ = false;
      }.bind(this);

      this.updateMergedDataPending_ = true;
      window.setTimeout(functionToRun, timeToWait);
    },

    /**
     * Returns a list of the currently selected snapshots. This list is
     * guaranteed to be of length 1 or 2.
     */
    getSelectedSnapshotIndexes_: function() {
      var indexes = this.getSelectedSnapshotBoxes_();
      for (var i = 0; i < indexes.length; ++i)
        indexes[i] = indexes[i].__index;
      return indexes;
    },

    /**
     * Same as getSelectedSnapshotIndexes_(), only it returns the actual
     * checkbox input DOM nodes rather than the snapshot ID.
     */
    getSelectedSnapshotBoxes_: function() {
      // Figure out which snaphots to use for our data.
      var boxes = [];
      for (var i = 0; i < this.snapshots_.length; ++i) {
        var box = this.getSnapshotCheckbox_(i);
        if (box.checked)
          boxes.push(box);
      }
      return boxes;
    },

    /**
     * Re-draw the description that explains which snapshots are currently
     * selected (if two snapshots were selected we explain that the *difference*
     * between them is being displayed).
     */
    updateSnapshotSelectionSummaryDiv_: function() {
      var summaryDiv = $(SNAPSHOT_SELECTION_SUMMARY_ID);

      var selectedSnapshots = this.getSelectedSnapshotIndexes_();
      if (selectedSnapshots.length == 0) {
        // This can occur during an attempt to load a file or following file
        // load failure.  We just ignore it and move on.
      } else if (selectedSnapshots.length == 1) {
        // If only one snapshot is chosen then we will display that snapshot's
        // data in its entirety.
        this.flatData_ = this.snapshots_[selectedSnapshots[0]].flatData;

        // Don't bother displaying any text when just 1 snapshot is selected,
        // since it is obvious what this should do.
        summaryDiv.innerText = '';
      } else if (selectedSnapshots.length == 2) {
        // Otherwise if two snapshots were chosen, show the difference between
        // them.
        var snapshot1 = this.snapshots_[selectedSnapshots[0]];
        var snapshot2 = this.snapshots_[selectedSnapshots[1]];

        var timeDeltaInSeconds =
            ((snapshot2.time - snapshot1.time) / 1000).toFixed(0);

        // Explain that what is being shown is the difference between two
        // snapshots.
        summaryDiv.innerText =
            'Showing the difference between snapshots #' +
            selectedSnapshots[0] + ' and #' +
            selectedSnapshots[1] + ' (' + timeDeltaInSeconds +
            ' seconds worth of data)';
      } else {
        // This shouldn't be possible...
        throw 'Unexpected number of selected snapshots';
      }
    },

    updateMergedData_: function() {
      // Retrieve the merge options.
      var mergeColumns = this.getMergeColumns_();
      var shouldMergeSimilarThreads = this.shouldMergeSimilarThreads_();

      var selectedSnapshots = this.getSelectedSnapshotIndexes_();

      // We do merges a bit differently depending if we are displaying the diffs
      // between two snapshots, or just displaying a single snapshot.
      if (selectedSnapshots.length == 1) {
        var snapshot = this.snapshots_[selectedSnapshots[0]];
        this.mergedData_ = mergeRows(snapshot.flatData,
                                     mergeColumns,
                                     shouldMergeSimilarThreads,
                                     false);

      } else if (selectedSnapshots.length == 2) {
        var snapshot1 = this.snapshots_[selectedSnapshots[0]];
        var snapshot2 = this.snapshots_[selectedSnapshots[1]];

        // Merge the data for snapshot1.
        var mergedRows1 = mergeRows(snapshot1.flatData,
                                    mergeColumns,
                                    shouldMergeSimilarThreads,
                                    true);

        // Merge the data for snapshot2.
        var mergedRows2 = mergeRows(snapshot2.flatData,
                                    mergeColumns,
                                    shouldMergeSimilarThreads,
                                    true);

        // Do a diff between the two snapshots.
        this.mergedData_ = subtractSnapshots(mergedRows1,
                                             mergedRows2,
                                             mergeColumns);
      } else {
        throw 'Unexpected number of selected snapshots';
      }

      // Recompute filteredData_ (since it is derived from mergedData_)
      this.updateFilteredData_();
    },

    updateFilteredData_: function() {
      // Recompute filteredData_.
      this.filteredData_ = [];
      var filterFunc = this.getFilterFunction_();
      for (var i = 0; i < this.mergedData_.length; ++i) {
        var r = this.mergedData_[i];
        if (!filterFunc(r)) {
          // Not matched by our filter, discard.
          continue;
        }
        this.filteredData_.push(r);
      }

      // Recompute groupedData_ (since it is derived from filteredData_)
      this.updateGroupedData_();
    },

    updateGroupedData_: function() {
      // Recompute groupedData_.
      var groupKeyToData = {};
      var entryToGroupKeyFunc = this.getGroupingFunction_();
      for (var i = 0; i < this.filteredData_.length; ++i) {
        var r = this.filteredData_[i];

        var groupKey = entryToGroupKeyFunc(r);

        var groupData = groupKeyToData[groupKey];
        if (!groupData) {
          groupData = {
            key: JSON.parse(groupKey),
            aggregates: initializeAggregates(ALL_KEYS),
            rows: [],
          };
          groupKeyToData[groupKey] = groupData;
        }

        // Add the row to our list.
        groupData.rows.push(r);

        // Update aggregates for each column.
        consumeAggregates(groupData.aggregates, r);
      }
      this.groupedData_ = groupKeyToData;

      // Figure out a display order for the groups themselves.
      this.sortedGroupKeys_ = getDictionaryKeys(groupKeyToData);
      this.sortedGroupKeys_.sort(this.getGroupSortingFunction_());

      // Sort the group data.
      this.sortGroupedData_();
    },

    sortGroupedData_: function() {
      var sortingFunc = this.getSortingFunction_();
      for (var k in this.groupedData_)
        this.groupedData_[k].rows.sort(sortingFunc);

      // Every cached data dependency is now up to date, all that is left is
      // to actually draw the result.
      this.redrawData_();
    },

    getVisibleColumnKeys_: function() {
      // Figure out what columns to include, based on the selected checkboxes.
      var columns = this.getSelectionColumns_();
      columns = columns.slice(0);

      // Eliminate columns which we are merging on.
      deleteValuesFromArray(columns, this.getMergeColumns_());

      // Eliminate columns which we are grouped on.
      if (this.sortedGroupKeys_.length > 0) {
        // The grouping will be the the same for each so just pick the first.
        var randomGroupKey = this.groupedData_[this.sortedGroupKeys_[0]].key;

        // The grouped properties are going to be the same for each row in our,
        // table, so avoid drawing them in our table!
        var keysToExclude = [];

        for (var i = 0; i < randomGroupKey.length; ++i)
          keysToExclude.push(randomGroupKey[i].key);
        deleteValuesFromArray(columns, keysToExclude);
      }

      // If we are currently showing a "diff", hide the max columns, since we
      // are not populating it correctly. See the TODO at the top of this file.
      if (this.getSelectedSnapshotIndexes_().length > 1)
        deleteValuesFromArray(columns, [KEY_MAX_RUN_TIME, KEY_MAX_QUEUE_TIME]);

      return columns;
    },

    redrawData_: function() {
      // Clear the results div, sine we may be overwriting older data.
      var parent = $(RESULTS_DIV_ID);
      parent.innerHTML = '';

      var columns = this.getVisibleColumnKeys_();

      // Draw each group.
      for (var i = 0; i < this.sortedGroupKeys_.length; ++i) {
        var k = this.sortedGroupKeys_[i];
        this.drawGroup_(parent, k, columns);
      }
    },

    /**
     * Renders the information for a particular group.
     */
    drawGroup_: function(parent, groupKey, columns) {
      var groupData = this.groupedData_[groupKey];

      var div = addNode(parent, 'div');
      div.className = 'group-container';

      this.drawGroupTitle_(div, groupData.key);

      var table = addNode(div, 'table');

      this.drawDataTable_(table, groupData, columns, groupKey);
    },

    /**
     * Draws a title into |parent| that describes |groupKey|.
     */
    drawGroupTitle_: function(parent, groupKey) {
      if (groupKey.length == 0) {
        // Empty group key means there was no grouping.
        return;
      }

      var parent = addNode(parent, 'div');
      parent.className = 'group-title-container';

      // Each component of the group key represents the "key=value" constraint
      // for this group. Show these as an AND separated list.
      for (var i = 0; i < groupKey.length; ++i) {
        if (i > 0)
          addNode(parent, 'i', ' and ');
        var e = groupKey[i];
        addNode(parent, 'b', getNameForKey(e.key) + ' = ');
        addNode(parent, 'span', e.value);
      }
    },

    /**
     * Renders a table which summarizes all |column| fields for |data|.
     */
    drawDataTable_: function(table, data, columns, groupKey) {
      table.className = 'results-table';
      var thead = addNode(table, 'thead');
      var tbody = addNode(table, 'tbody');

      var displaySettings = this.getGroupDisplaySettings_(groupKey);
      var limit = displaySettings.limit;

      this.drawAggregateRow_(thead, data.aggregates, columns);
      this.drawTableHeader_(thead, columns);
      this.drawTableBody_(tbody, data.rows, columns, limit);
      this.drawTruncationRow_(tbody, data.rows.length, limit, columns.length,
                              groupKey);
    },

    drawTableHeader_: function(thead, columns) {
      var tr = addNode(thead, 'tr');
      for (var i = 0; i < columns.length; ++i) {
        var key = columns[i];
        var th = addNode(tr, 'th', getNameForKey(key));
        th.onclick = this.onClickColumn_.bind(this, key);

        // Draw an indicator if we are currently sorted on this column.
        // TODO(eroman): Should use an icon instead of asterisk!
        for (var j = 0; j < this.currentSortKeys_.length; ++j) {
          if (sortKeysMatch(this.currentSortKeys_[j], key)) {
            var sortIndicator = addNode(th, 'span', '*');
            sortIndicator.style.color = 'red';
            if (sortKeyIsReversed(this.currentSortKeys_[j])) {
              // Use double-asterisk for descending columns.
              addText(sortIndicator, '*');
            }
            break;
          }
        }
      }
    },

    drawTableBody_: function(tbody, rows, columns, limit) {
      for (var i = 0; i < rows.length && i < limit; ++i) {
        var e = rows[i];

        var tr = addNode(tbody, 'tr');

        for (var c = 0; c < columns.length; ++c) {
          var key = columns[c];
          var value = e[key];

          var td = addNode(tr, 'td');
          drawValueToCell(td, key, value);
        }
      }
    },

    /**
     * Renders a row that describes all the aggregate values for |columns|.
     */
    drawAggregateRow_: function(tbody, aggregates, columns) {
      var tr = addNode(tbody, 'tr');
      tr.className = 'aggregator-row';

      for (var i = 0; i < columns.length; ++i) {
        var key = columns[i];
        var td = addNode(tr, 'td');

        // Most of our outputs are numeric, so we want to align them to the
        // right. However for the  unique counts we will center.
        if (KEY_PROPERTIES[key].aggregator == UniquifyAggregator) {
          td.align = 'center';
        } else {
          td.align = 'right';
        }

        var aggregator = aggregates[key];
        if (aggregator)
          td.innerText = aggregator.getValueAsText();
      }
    },

    /**
     * Renders a row which describes how many rows the table has, how many are
     * currently hidden, and a set of buttons to show more.
     */
    drawTruncationRow_: function(tbody, numRows, limit, numColumns, groupKey) {
      var numHiddenRows = Math.max(numRows - limit, 0);
      var numVisibleRows = numRows - numHiddenRows;

      var tr = addNode(tbody, 'tr');
      tr.className = 'truncation-row';
      var td = addNode(tr, 'td');
      td.colSpan = numColumns;

      addText(td, numRows + ' rows');
      if (numHiddenRows > 0) {
        var s = addNode(td, 'span', ' (' + numHiddenRows + ' hidden) ');
        s.style.color = 'red';
      }

      if (numVisibleRows > LIMIT_INCREMENT) {
        addNode(td, 'button', 'Show less').onclick =
            this.changeGroupDisplayLimit_.bind(
                this, groupKey, -LIMIT_INCREMENT);
      }
      if (numVisibleRows > 0) {
        addNode(td, 'button', 'Show none').onclick =
            this.changeGroupDisplayLimit_.bind(this, groupKey, -Infinity);
      }

      if (numHiddenRows > 0) {
        addNode(td, 'button', 'Show more').onclick =
            this.changeGroupDisplayLimit_.bind(this, groupKey, LIMIT_INCREMENT);
        addNode(td, 'button', 'Show all').onclick =
            this.changeGroupDisplayLimit_.bind(this, groupKey, Infinity);
      }
    },

    /**
     * Adjusts the row limit for group |groupKey| by |delta|.
     */
    changeGroupDisplayLimit_: function(groupKey, delta) {
      // Get the current settings for this group.
      var settings = this.getGroupDisplaySettings_(groupKey, true);

      // Compute the adjusted limit.
      var newLimit = settings.limit;
      var totalNumRows = this.groupedData_[groupKey].rows.length;
      newLimit = Math.min(totalNumRows, newLimit);
      newLimit += delta;
      newLimit = Math.max(0, newLimit);

      // Update the settings with the new limit.
      settings.limit = newLimit;

      // TODO(eroman): It isn't necessary to redraw *all* the data. Really we
      // just need to insert the missing rows (everything else stays the same)!
      this.redrawData_();
    },

    /**
     * Returns the rendering settings for group |groupKey|. This includes things
     * like how many rows to display in the table.
     */
    getGroupDisplaySettings_: function(groupKey, opt_create) {
      var settings = this.groupDisplaySettings_[groupKey];
      if (!settings) {
        // If we don't have any settings for this group yet, create some
        // default ones.
        if (groupKey == '[]') {
          // (groupKey of '[]' is what we use for ungrouped data).
          settings = {limit: INITIAL_UNGROUPED_ROW_LIMIT};
        } else {
          settings = {limit: INITIAL_GROUP_ROW_LIMIT};
        }
        if (opt_create)
          this.groupDisplaySettings_[groupKey] = settings;
      }
      return settings;
    },

    init_: function() {
      this.snapshots_ = [];

      // Start fetching the data from the browser; this will be our snapshot #0.
      this.takeSnapshot_();

      // Data goes through the following pipeline:
      // (1) Raw data received from browser, and transformed into our own
      //     internal row format (where properties are indexed by KEY_*
      //     constants.)
      // (2) We "augment" each row by adding some extra computed columns
      //     (like averages).
      // (3) The rows are merged using current merge settings.
      // (4) The rows that don't match current search expression are
      //     tossed out.
      // (5) The rows are organized into "groups" based on current settings,
      //     and aggregate values are computed for each resulting group.
      // (6) The rows within each group are sorted using current settings.
      // (7) The grouped rows are drawn to the screen.
      this.mergedData_ = [];
      this.filteredData_ = [];
      this.groupedData_ = {};
      this.sortedGroupKeys_ = [];

      this.groupDisplaySettings_ = {};

      this.fillSelectionCheckboxes_($(COLUMN_TOGGLES_CONTAINER_ID));
      this.fillMergeCheckboxes_($(COLUMN_MERGE_TOGGLES_CONTAINER_ID));

      $(FILTER_SEARCH_ID).onsearch = this.onChangedFilter_.bind(this);

      this.currentSortKeys_ = INITIAL_SORT_KEYS.slice(0);
      this.currentGroupingKeys_ = INITIAL_GROUP_KEYS.slice(0);

      this.fillGroupingDropdowns_();
      this.fillSortingDropdowns_();

      $(EDIT_COLUMNS_LINK_ID).onclick =
          toggleNodeDisplay.bind(null, $(EDIT_COLUMNS_ROW));

      $(TOGGLE_SNAPSHOTS_LINK_ID).onclick =
          toggleNodeDisplay.bind(null, $(SNAPSHOTS_ROW));

      $(MERGE_SIMILAR_THREADS_CHECKBOX_ID).onchange =
          this.onMergeSimilarThreadsCheckboxChanged_.bind(this);

      $(RESET_DATA_LINK_ID).onclick =
          g_browserBridge.sendResetData.bind(g_browserBridge);

      $(TAKE_SNAPSHOT_BUTTON_ID).onclick = this.takeSnapshot_.bind(this);

      $(SAVE_SNAPSHOTS_BUTTON_ID).onclick = this.saveSnapshots_.bind(this);
      $(SNAPSHOT_FILE_LOADER_ID).onchange = this.loadFileChanged_.bind(this);
    },

    takeSnapshot_: function() {
      // Start a new empty snapshot. Make note of the current time, so we know
      // when the snaphot was taken.
      this.snapshots_.push({flatData: [], origData: [], time: getTimeMillis()});

      // Update the UI to reflect the new snapshot.
      this.addSnapshotToList_(this.snapshots_.length - 1);

      // Ask the browser for the profiling data. We will receive the data
      // later through a callback to addDataToSnapshot_().
      g_browserBridge.sendGetData();
    },

    saveSnapshots_: function() {
      var snapshots = [];
      for (var i = 0; i < this.snapshots_.length; ++i) {
        snapshots.push({ data: this.snapshots_[i].origData,
                         timestamp: Math.floor(
                                 this.snapshots_[i].time / 1000) });
      }

      var dump = {
        'userAgent': navigator.userAgent,
        'version': 1,
        'snapshots': snapshots
      };

      var dumpText = JSON.stringify(dump, null, ' ');
      var textBlob = new Blob([dumpText],
                              { type: 'octet/stream', endings: 'native' });
      var blobUrl = window.URL.createObjectURL(textBlob);
      $(DOWNLOAD_ANCHOR_ID).href = blobUrl;
      $(DOWNLOAD_ANCHOR_ID).click();
    },

    loadFileChanged_: function() {
      this.loadSnapshots_($(SNAPSHOT_FILE_LOADER_ID).files[0]);
    },

    loadSnapshots_: function(file) {
      if (file) {
        var fileReader = new FileReader();

        fileReader.onload = this.onLoadSnapshotsFile_.bind(this, file);
        fileReader.onerror = this.onLoadSnapshotsFileError_.bind(this, file);

        fileReader.readAsText(file);
      }
    },

    onLoadSnapshotsFile_: function(file, event) {
      try {
        var parsed = null;
        parsed = JSON.parse(event.target.result);

        if (parsed.version != 1) {
          throw new Error('Unrecognized version: ' + parsed.version);
        }

        if (parsed.snapshots.length < 1) {
          throw new Error('File contains no data');
        }

        this.displayLoadedFile_(file, parsed);
        this.hideFileLoadError_();
      } catch (error) {
        this.displayFileLoadError_('File load failure: ' + error.message);
      }
    },

    clearExistingSnapshots_: function() {
      var tbody = $('snapshots-tbody');
      this.snapshots_ = [];
      tbody.innerHTML = '';
      this.updateMergedDataSoon_();
    },

    displayLoadedFile_: function(file, content) {
      this.clearExistingSnapshots_();
      $(TAKE_SNAPSHOT_BUTTON_ID).disabled = true;
      $(SAVE_SNAPSHOTS_BUTTON_ID).disabled = true;

      if (content.snapshots.length > 1) {
        setNodeDisplay($(SNAPSHOTS_ROW), true);
      }

      for (var i = 0; i < content.snapshots.length; ++i) {
        var snapshot = content.snapshots[i];
        this.snapshots_.push({flatData: [], origData: [],
                              time: snapshot.timestamp * 1000});
        this.addSnapshotToList_(this.snapshots_.length - 1);
        var snapshotData = snapshot.data;
        for (var j = 0; j < snapshotData.length; ++j) {
          this.addDataToSnapshot(snapshotData[j]);
        }
      }
      this.redrawData_();
    },

    onLoadSnapshotsFileError_: function(file, filedata) {
      this.displayFileLoadError_('Error loading ' + file.name);
    },

    displayFileLoadError_: function(message) {
      $(LOAD_ERROR_ID).textContent = message;
      $(LOAD_ERROR_ID).hidden = false;
    },

    hideFileLoadError_: function() {
      $(LOAD_ERROR_ID).textContent = '';
      $(LOAD_ERROR_ID).hidden = true;
    },

    getSnapshotCheckbox_: function(i) {
      return $(this.getSnapshotCheckboxId_(i));
    },

    getSnapshotCheckboxId_: function(i) {
      return 'snapshotCheckbox-' + i;
    },

    addSnapshotToList_: function(i) {
      var tbody = $('snapshots-tbody');

      var tr = addNode(tbody, 'tr');

      var id = this.getSnapshotCheckboxId_(i);

      var checkboxCell = addNode(tr, 'td');
      var checkbox = addNode(checkboxCell, 'input');
      checkbox.type = 'checkbox';
      checkbox.id = id;
      checkbox.__index = i;
      checkbox.onclick = this.onSnapshotCheckboxChanged_.bind(this);

      addNode(tr, 'td', '#' + i);

      var labelCell = addNode(tr, 'td');
      var l = addNode(labelCell, 'label');

      var dateString = new Date(this.snapshots_[i].time).toLocaleString();
      addText(l, dateString);
      l.htmlFor = id;

      // If we are on snapshot 0, make it the default.
      if (i == 0) {
        checkbox.checked = true;
        checkbox.__time = getTimeMillis();
        this.updateSnapshotCheckboxStyling_();
      }
    },

    updateSnapshotCheckboxStyling_: function() {
      for (var i = 0; i < this.snapshots_.length; ++i) {
        var checkbox = this.getSnapshotCheckbox_(i);
        checkbox.parentNode.parentNode.className =
            checkbox.checked ? 'selected_snapshot' : '';
      }
    },

    onSnapshotCheckboxChanged_: function(event) {
      // Keep track of when we clicked this box (for when we need to uncheck
      // older boxes).
      event.target.__time = getTimeMillis();

      // Find all the checked boxes. Either 1 or 2 can be checked. If a third
      // was just checked, then uncheck one of the earlier ones so we only have
      // 2.
      var checked = this.getSelectedSnapshotBoxes_();
      checked.sort(function(a, b) { return b.__time - a.__time; });
      if (checked.length > 2) {
        for (var i = 2; i < checked.length; ++i)
          checked[i].checked = false;
        checked.length = 2;
      }

      // We should always have at least 1 selection. Prevent the user from
      // unselecting the final box.
      if (checked.length == 0)
        event.target.checked = true;

      this.updateSnapshotCheckboxStyling_();
      this.updateSnapshotSelectionSummaryDiv_();

      // Recompute mergedData_ (since it is derived from selected snapshots).
      this.updateMergedData_();
    },

    fillSelectionCheckboxes_: function(parent) {
      this.selectionCheckboxes_ = {};

      var onChangeFunc = this.onSelectCheckboxChanged_.bind(this);

      for (var i = 0; i < ALL_TABLE_COLUMNS.length; ++i) {
        var key = ALL_TABLE_COLUMNS[i];
        var checkbox = addLabeledCheckbox(parent, getNameForKey(key));
        checkbox.checked = true;
        checkbox.onchange = onChangeFunc;
        addText(parent, ' ');
        this.selectionCheckboxes_[key] = checkbox;
      }

      for (var i = 0; i < INITIALLY_HIDDEN_KEYS.length; ++i) {
        this.selectionCheckboxes_[INITIALLY_HIDDEN_KEYS[i]].checked = false;
      }
    },

    getSelectionColumns_: function() {
      return getKeysForCheckedBoxes(this.selectionCheckboxes_);
    },

    getMergeColumns_: function() {
      return getKeysForCheckedBoxes(this.mergeCheckboxes_);
    },

    shouldMergeSimilarThreads_: function() {
      return $(MERGE_SIMILAR_THREADS_CHECKBOX_ID).checked;
    },

    fillMergeCheckboxes_: function(parent) {
      this.mergeCheckboxes_ = {};

      var onChangeFunc = this.onMergeCheckboxChanged_.bind(this);

      for (var i = 0; i < MERGEABLE_KEYS.length; ++i) {
        var key = MERGEABLE_KEYS[i];
        var checkbox = addLabeledCheckbox(parent, getNameForKey(key));
        checkbox.onchange = onChangeFunc;
        addText(parent, ' ');
        this.mergeCheckboxes_[key] = checkbox;
      }

      for (var i = 0; i < INITIALLY_MERGED_KEYS.length; ++i) {
        this.mergeCheckboxes_[INITIALLY_MERGED_KEYS[i]].checked = true;
      }
    },

    fillGroupingDropdowns_: function() {
      var parent = $(GROUP_BY_CONTAINER_ID);
      parent.innerHTML = '';

      for (var i = 0; i <= this.currentGroupingKeys_.length; ++i) {
        // Add a dropdown.
        var select = addNode(parent, 'select');
        select.onchange = this.onChangedGrouping_.bind(this, select, i);

        addOptionsForGroupingSelect(select);

        if (i < this.currentGroupingKeys_.length) {
          var key = this.currentGroupingKeys_[i];
          setSelectedOptionByValue(select, key);
        }
      }
    },

    fillSortingDropdowns_: function() {
      var parent = $(SORT_BY_CONTAINER_ID);
      parent.innerHTML = '';

      for (var i = 0; i <= this.currentSortKeys_.length; ++i) {
        // Add a dropdown.
        var select = addNode(parent, 'select');
        select.onchange = this.onChangedSorting_.bind(this, select, i);

        addOptionsForSortingSelect(select);

        if (i < this.currentSortKeys_.length) {
          var key = this.currentSortKeys_[i];
          setSelectedOptionByValue(select, key);
        }
      }
    },

    onChangedGrouping_: function(select, i) {
      updateKeyListFromDropdown(this.currentGroupingKeys_, i, select);
      this.fillGroupingDropdowns_();
      this.updateGroupedData_();
    },

    onChangedSorting_: function(select, i) {
      updateKeyListFromDropdown(this.currentSortKeys_, i, select);
      this.fillSortingDropdowns_();
      this.sortGroupedData_();
    },

    onSelectCheckboxChanged_: function() {
      this.redrawData_();
    },

    onMergeCheckboxChanged_: function() {
      this.updateMergedData_();
    },

    onMergeSimilarThreadsCheckboxChanged_: function() {
      this.updateMergedData_();
    },

    onChangedFilter_: function() {
      this.updateFilteredData_();
    },

    /**
     * When left-clicking a column, change the primary sort order to that
     * column. If we were already sorted on that column then reverse the order.
     *
     * When alt-clicking, add a secondary sort column. Similarly, if
     * alt-clicking a column which was already being sorted on, reverse its
     * order.
     */
    onClickColumn_: function(key, event) {
      // If this property wants to start off in descending order rather then
      // ascending, flip it.
      if (KEY_PROPERTIES[key].sortDescending)
        key = reverseSortKey(key);

      // Scan through our sort order and see if we are already sorted on this
      // key. If so, reverse that sort ordering.
      var foundIndex = -1;
      for (var i = 0; i < this.currentSortKeys_.length; ++i) {
        var curKey = this.currentSortKeys_[i];
        if (sortKeysMatch(curKey, key)) {
          this.currentSortKeys_[i] = reverseSortKey(curKey);
          foundIndex = i;
          break;
        }
      }

      if (event.altKey) {
        if (foundIndex == -1) {
          // If we weren't already sorted on the column that was alt-clicked,
          // then add it to our sort.
          this.currentSortKeys_.push(key);
        }
      } else {
        if (foundIndex != 0 ||
            !sortKeysMatch(this.currentSortKeys_[foundIndex], key)) {
          // If the column we left-clicked wasn't already our primary column,
          // make it so.
          this.currentSortKeys_ = [key];
        } else {
          // If the column we left-clicked was already our primary column (and
          // we just reversed it), remove any secondary sorts.
          this.currentSortKeys_.length = 1;
        }
      }

      this.fillSortingDropdowns_();
      this.sortGroupedData_();
    },

    getSortingFunction_: function() {
      var sortKeys = this.currentSortKeys_.slice(0);

      // Eliminate the empty string keys (which means they were unspecified).
      deleteValuesFromArray(sortKeys, ['']);

      // If no sort is specified, use our default sort.
      if (sortKeys.length == 0)
        sortKeys = [DEFAULT_SORT_KEYS];

      return function(a, b) {
        for (var i = 0; i < sortKeys.length; ++i) {
          var key = Math.abs(sortKeys[i]);
          var factor = sortKeys[i] < 0 ? -1 : 1;

          var propA = a[key];
          var propB = b[key];

          var comparison = compareValuesForKey(key, propA, propB);
          comparison *= factor;  // Possibly reverse the ordering.

          if (comparison != 0)
            return comparison;
        }

        // Tie breaker.
        return simpleCompare(JSON.stringify(a), JSON.stringify(b));
      };
    },

    getGroupSortingFunction_: function() {
      return function(a, b) {
        var groupKey1 = JSON.parse(a);
        var groupKey2 = JSON.parse(b);

        for (var i = 0; i < groupKey1.length; ++i) {
          var comparison = compareValuesForKey(
              groupKey1[i].key,
              groupKey1[i].value,
              groupKey2[i].value);

          if (comparison != 0)
            return comparison;
        }

        // Tie breaker.
        return simpleCompare(a, b);
      };
    },

    getFilterFunction_: function() {
      var searchStr = $(FILTER_SEARCH_ID).value;

      // Normalize the search expression.
      searchStr = trimWhitespace(searchStr);
      searchStr = searchStr.toLowerCase();

      return function(x) {
        // Match everything when there was no filter.
        if (searchStr == '')
          return true;

        // Treat the search text as a LOWERCASE substring search.
        for (var k = BEGIN_KEY; k < END_KEY; ++k) {
          var propertyText = getTextValueForProperty(k, x[k]);
          if (propertyText.toLowerCase().indexOf(searchStr) != -1)
            return true;
        }

        return false;
      };
    },

    getGroupingFunction_: function() {
      var groupings = this.currentGroupingKeys_.slice(0);

      // Eliminate the empty string groupings (which means they were
      // unspecified).
      deleteValuesFromArray(groupings, ['']);

      // Eliminate duplicate primary/secondary group by directives, since they
      // are redundant.
      deleteDuplicateStringsFromArray(groupings);

      return function(e) {
        var groupKey = [];

        for (var i = 0; i < groupings.length; ++i) {
          var entry = {key: groupings[i],
                       value: e[groupings[i]]};
          groupKey.push(entry);
        }

        return JSON.stringify(groupKey);
      };
    },
  };

  return MainView;
})();
