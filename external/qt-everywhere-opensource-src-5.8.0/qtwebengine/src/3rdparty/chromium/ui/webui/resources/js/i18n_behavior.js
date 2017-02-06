// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nBehavior' is a behavior to mix in loading of
 * internationalization strings.
 *
 * Example:
 *   behaviors: [
 *     I18nBehavior,
 *   ],
 */

/** @polymerBehavior */
var I18nBehavior = {
  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values.
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, substituted string.
   * @private
   */
  i18nRaw_: function(id, var_args) {
    return arguments.length == 1 ? loadTimeData.getString(id) :
        loadTimeData.getStringF.apply(loadTimeData, arguments);
  },

  /**
   * Returns a translated string where $1 to $9 are replaced by the given
   * values. Also sanitizes the output to filter out dangerous HTML/JS.
   * @param {string} id The ID of the string to translate.
   * @param {...string} var_args Values to replace the placeholders $1 to $9
   *     in the string.
   * @return {string} A translated, sanitized, substituted string.
   */
  i18n: function(id, var_args) {
    var rawString = this.i18nRaw_.apply(this, arguments);
    return parseHtmlSubset('<b>' + rawString + '</b>').firstChild.innerHTML;
  },

  /**
   * Similar to 'i18n', returns a translated, sanitized, substituted string.
   * It receives the string ID and a dictionary containing the substitutions
   * as well as optional additional allowed tags and attributes.
   * @param {string} id The ID of the string to translate.
   * @param {{substitutions: (Array<string>|undefined),
   *          attrs: (Object<function(Node, string):boolean>|undefined),
   *          tags: (Array<string>|undefined)}} opts
   */
  i18nAdvanced: function(id, opts) {
    var args = [id].concat(opts.substitutions || []);
    var rawString = this.i18nRaw_.apply(this, args);
    return parseHtmlSubset('<b>' + rawString + '</b>', opts.tags, opts.attrs)
        .firstChild.innerHTML;
  },
};
