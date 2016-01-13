// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Asserts that a given argument's value is undefined.
 * @param {object} a The argument to check.
 */
function assertUndefined(a) {
  if (a !== undefined) {
    throw new Error('Assertion failed: expected undefined');
  }
}

/**
 * Asserts that a given function call raises an exception.
 * @param {string} msg The message to print if this fails.
 * @param {Function} fn The function to call.
 * @param {string} error The name of the exception we expect to throw.
 * @return {boolean} True if it throws the exception.
 */
function assertException(msg, fn, error) {
  try {
    fn();
  } catch (e) {
    if (error && e.name != error) {
      throw new Error('Expected to throw ' + error + ' but threw ' + e.name +
          ' - ' + msg);
    }

    return true;
  }

  throw new Error('Expected to throw exception');
}

/**
 * Asserts that two arrays of strings are equal.
 * @param {Array.<string>} array1 The expected array.
 * @param {Array.<string>} array2 The test array.
 */
function assertEqualStringArrays(array1, array2) {
  var same = true;
  if (array1.length != array2.length) {
    same = false;
  }
  for (var i = 0; i < Math.min(array1.length, array2.length); i++) {
    if (array1[i].trim() != array2[i].trim()) {
      same = false;
    }
  }
  if (!same) {
    throw new Error('Expected ' + JSON.stringify(array1) +
                    ', got ' + JSON.stringify(array2));
  }
}

/**
 * Asserts that two objects have the same JSON serialization.
 * @param {Object} obj1 The expected object.
 * @param {Object} obj2 The actual object.
 */
function assertEqualsJSON(obj1, obj2) {
  if (!eqJSON(obj1, obj2)) {
    throw new Error('Expected ' + JSON.stringify(obj1) +
                    ', got ' + JSON.stringify(obj2));
  }
}

assertSame = assertEquals;
assertNotSame = assertNotEquals;

/**
 * Base test fixture for ChromeVox unit tests.
 *
 * Note that while conceptually these are unit tests, these tests need
 * to run in a full web page, so they're actually run as WebUI browser
 * tests.
 *
 * @constructor
 * @extends {testing.Test}
 */
function ChromeVoxUnitTestBase() {}

ChromeVoxUnitTestBase.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: DUMMY_URL,

  /**
   * @override
   * It doesn't make sense to run the accessibility audit on these tests,
   * since many of them are deliberately testing inaccessible html.
   */
  runAccessibilityChecks: false,

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously.
   * @param {string} html The html to load as a string.
   */
  loadHtml: function(html) {
    document.open();
    document.write(html);
    document.close();
  },

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously. This version takes the html
   * encoded as a comment inside a function, so you can use it like this:
   *
   * this.loadDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  loadDoc: function(commentEncodedHtml) {
    var html = commentEncodedHtml.toString().
        replace(/^[^\/]+\/\*!?/, '').
        replace(/\*\/[^\/]+$/, '');
    this.loadHtml(html);
  }
};
