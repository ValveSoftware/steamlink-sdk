// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  /** @const */ var EditableTextField = options.EditableTextField;

  /**
   * The regular expression that matches an IP address. String to match against
   * should have all whitespace stripped already.
   * @const
   * @type {RegExp}
   */
  var singleIp_ = /^([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)$/;

  /**
   * Creates a new field specifically for entering IP addresses.
   * @constructor
   */
  function IPAddressField() {
    var el = cr.doc.createElement('div');
    IPAddressField.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a inline-editable list item. Note that this is
   * a subclass of IPAddressField.
   * @param {!HTMLElement} el The element to decorate.
   */
  IPAddressField.decorate = function(el) {
    el.__proto__ = IPAddressField.prototype;
    el.decorate();
  };

  IPAddressField.prototype = {
    __proto__: EditableTextField.prototype,

    /** @override */
    decorate: function() {
      EditableTextField.prototype.decorate.call(this);
    },

    /**
     * Indicates whether or not empty values are allowed.
     * @type {boolean}
     */
    get allowEmpty() {
      return this.hasAttribute('allow-empty');
    },

    /** @override */
    get currentInputIsValid() {
      if (!this.editField.value && this.allowEmpty)
        return true;

      // Make sure it's only got numbers and ".", there are the correct
      // count of them, and they are all within the correct range.
      var fieldValue = this.editField.value.replace(/\s/g, '');
      var matches = singleIp_.exec(fieldValue);
      var rangeCorrect = true;
      if (matches != null) {
        for (var i = 1; i < matches.length; ++i) {
          var value = parseInt(matches[i], 10);
          if (value < 0 || value > 255) {
            rangeCorrect = false;
            break;
          }
        }
      }
      return this.editField.validity.valid && matches != null &&
          rangeCorrect && matches.length == 5;
    },

    /** @override */
    get hasBeenEdited() {
      return this.editField.value != this.model.value;
    },

    /**
     * Overrides superclass to mutate the input during a successful commit. For
     * the purposes of entering IP addresses, this just means stripping off
     * whitespace and leading zeros from each of the octets so that they conform
     * to the normal format for IP addresses.
     * @override
     * @param {string} value Input IP address to be mutated.
     * @return {string} mutated IP address.
     */
    mutateInput: function(value) {
      if (!value)
        return value;

      var fieldValue = value.replace(/\s/g, '');
      var matches = singleIp_.exec(fieldValue);
      var result = [];

      // If we got this far, matches shouldn't be null, but make sure.
      if (matches != null) {
        // starting at one because the first match element contains the entire
        // match, and we don't care about that.
        for (var i = 1; i < matches.length; ++i)
          result.push(parseInt(matches[i], 10));
      }
      return result.join('.');
    },
  };

  return {
    IPAddressField: IPAddressField,
  };
});
