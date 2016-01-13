// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Holds information about a braille table.
 */

goog.provide('cvox.BrailleTable');


/**
 * @typedef {{
 *   locale:string,
 *   dots:string,
 *   id:string,
 *   grade:(string|undefined),
 *   fileName:string
 * }}
 */
cvox.BrailleTable.Table;


/**
 * @const {string}
 */
cvox.BrailleTable.TABLE_PATH = 'chromevox/background/braille/tables.json';


/**
 * Retrieves a list of all available braille tables.
 * @param {function(!Array.<cvox.BrailleTable.Table>)} callback
 */
cvox.BrailleTable.getAll = function(callback) {
  var url = chrome.extension.getURL(cvox.BrailleTable.TABLE_PATH);
  if (!url) {
    throw 'Invalid path: ' + cvox.BrailleTable.TABLE_PATH;
  }

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      if (xhr.status == 200) {
        callback(/** @type {!Array.<cvox.BrailleTable.Table>} */ (
            JSON.parse(xhr.responseText)));
      }
    }
  };
  xhr.send();
};


/**
 * Finds a table in a list of tables by id.
 * @param {!Array.<cvox.BrailleTable.Table>} tables tables to search in.
 * @param {string} id id of table to find.
 * @return {cvox.BrailleTable.Table}
 */
cvox.BrailleTable.forId = function(tables, id) {
  return tables.filter(function(table) { return table.id === id })[0] || null;
};


/**
 * Returns an uncontracted braille table corresponding to another, possibly
 * contracted, table.  If {@code table} is the lowest-grade table for its
 * locale and dot count, {@code table} itself is returned.
 * @param {!Array.<cvox.BrailleTable.Table>} tables tables to search in.
 * @param {!cvox.BrailleTable.Table} table Table to match.
 * @return {!cvox.BrailleTable.Table} Corresponding uncontracted table,
 *     or {@code table} if it is uncontracted.
 */
cvox.BrailleTable.getUncontracted = function(tables, table) {
  function mostUncontractedOf(current, candidate) {
    // An 8 dot table for the same language is prefered over a 6 dot table
    // even if the locales differ by region.
    if (current.dots === '6' &&
        candidate.dots === '8' &&
        current.locale.lastIndexOf(candidate.locale, 0) == 0) {
      return candidate;
    }
    if (current.locale === candidate.locale &&
        current.dots === candidate.dots &&
        goog.isDef(current.grade) &&
        goog.isDef(candidate.grade) &&
        candidate.grade < current.grade) {
      return candidate;
    }
    return current;
  }
  return tables.reduce(mostUncontractedOf, table);
};
