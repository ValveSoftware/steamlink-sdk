// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * NOTE: The use of this file is deprecated. Use load_time_data.js instead.
 *
 * The local strings get injected into the page using a variable named
 * {@code templateData}. This class provides a simpler interface to access those
 * strings.
 *
 * @param {Object} opt_templateData Optional object containing translated
 *     strings.  If this is not supplied during construction, it can be
 *     assigned to the templateData property after construction.  If all else
 *     fails, the value of window.templateDate will be used.
 * @constructor
 */
function LocalStrings(opt_templateData) {
  this.templateData = opt_templateData;
}

// Start of anonymous namespace.
(function() {

/**
 * Returns a formatted string where $1 to $9 are replaced by the second to the
 * tenth argument.
 * @param {string} s The format string.
 * @param {...string} The extra values to include in the formatted output.
 * @return {string} The string after format substitution.
 */
function replaceArgs(s, args) {
  return s.replace(/\$[$1-9]/g, function(m) {
    return (m == '$$') ? '$' : args[m[1]];
  });
}

/**
 * Returns a string after removing Windows-style accelerators.
 * @param {string} s The input string that may contain accelerators.
 * @return {string} The resulting string with accelerators removed.
 */
function trimAccelerators(s) {
  return s.replace(/&{1,2}/g, function(m) {
    return (m == '&&') ? '&' : '';
  });
}

LocalStrings.prototype = {
  /**
   * The template data object.
   * @type {Object}
   */
  templateData: null,

  /**
   * Gets a localized string by its id.
   * @param {string} s The ID of the string we want.
   * @return {string} The localized string.
   */
  getString: function(id) {
    // TODO(arv): We should not rely on a global variable here.
    var templateData = this.templateData || window.templateData;
    var str = templateData[id];
    // TODO(jhawkins): Change to console.error when all errors are fixed.
    if (!str)
      console.warn('Missing string for id: ' + id);
    return str;
  },

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument.
   * @param {string} id The ID of the string we want.
   * @param {...string} The extra values to include in the formatted output.
   * @return {string} The formatted string.
   */
  getStringF: function(id, var_args) {
    return replaceArgs(this.getString(id), arguments);
  },
};

// End of anonymous namespace.
})();
