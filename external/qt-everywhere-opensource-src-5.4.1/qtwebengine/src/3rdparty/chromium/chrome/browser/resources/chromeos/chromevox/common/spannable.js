// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class which allows construction of annotated strings.
 */

goog.provide('cvox.Spannable');

goog.require('goog.object');

/**
 * @constructor
 * @param {string=} opt_string Initial value of the spannable.
 * @param {*=} opt_annotation Initial annotation for the entire string.
 */
cvox.Spannable = function(opt_string, opt_annotation) {
  /**
   * Underlying string.
   * @type {string}
   * @private
   */
  this.string_ = opt_string || '';

  /**
   * Spans (annotations).
   * @type {!Array.<!{ value: *, start: number, end: number }>}
   * @private
   */
  this.spans_ = [];

  // Optionally annotate the entire string.
  if (goog.isDef(opt_annotation)) {
    var len = this.string_.length;
    this.spans_.push({ value: opt_annotation, start: 0, end: len });
  }
};


/** @override */
cvox.Spannable.prototype.toString = function() {
  return this.string_;
};


/**
 * Returns the length of the string.
 * @return {number} Length of the string.
 */
cvox.Spannable.prototype.getLength = function() {
  return this.string_.length;
};


/**
 * Adds a span to some region of the string.
 * @param {*} value Annotation.
 * @param {number} start Starting index (inclusive).
 * @param {number} end Ending index (exclusive).
 */
cvox.Spannable.prototype.setSpan = function(value, start, end) {
  this.removeSpan(value);
  if (0 <= start && start <= end && end <= this.string_.length) {
    // Zero-length spans are explicitly allowed, because it is possible to
    // query for position by annotation as well as the reverse.
    this.spans_.push({ value: value, start: start, end: end });
  } else {
    throw new RangeError('span out of range (start=' + start +
        ', end=' + end + ', len=' + this.string_.length + ')');
  }
};


/**
 * Removes a span.
 * @param {*} value Annotation.
 */
cvox.Spannable.prototype.removeSpan = function(value) {
  for (var i = this.spans_.length - 1; i >= 0; i--) {
    if (this.spans_[i].value === value) {
      this.spans_.splice(i, 1);
    }
  }
};


/**
 * Appends another Spannable or string to this one.
 * @param {string|!cvox.Spannable} other String or spannable to concatenate.
 */
cvox.Spannable.prototype.append = function(other) {
  if (other instanceof cvox.Spannable) {
    var otherSpannable = /** @type {!cvox.Spannable} */ (other);
    var originalLength = this.getLength();
    this.string_ += otherSpannable.string_;
    other.spans_.forEach(goog.bind(function(span) {
      this.setSpan(
          span.value,
          span.start + originalLength,
          span.end + originalLength);
    }, this));
  } else if (typeof other === 'string') {
    this.string_ += /** @type {string} */ (other);
  }
};


/**
 * Returns the first value matching a position.
 * @param {number} position Position to query.
 * @return {*} Value annotating that position, or undefined if none is found.
 */
cvox.Spannable.prototype.getSpan = function(position) {
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.start <= position && position < span.end) {
      return span.value;
    }
  }
};


/**
 * Returns the first span value which is an instance of a given constructor.
 * @param {!Function} constructor Constructor.
 * @return {!Object|undefined} Object if found; undefined otherwise.
 */
cvox.Spannable.prototype.getSpanInstanceOf = function(constructor) {
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.value instanceof constructor) {
      return span.value;
    }
  }
};


/**
 * Returns all spans matching a position.
 * @param {number} position Position to query.
 * @return {!Array} Values annotating that position.
 */
cvox.Spannable.prototype.getSpans = function(position) {
  var results = [];
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.start <= position && position < span.end) {
      results.push(span.value);
    }
  }
  return results;
};


/**
 * Returns the start of the requested span.
 * @param {*} value Annotation.
 * @return {number|undefined} Start of the span, or undefined if not attached.
 */
cvox.Spannable.prototype.getSpanStart = function(value) {
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.value === value) {
      return span.start;
    }
  }
  return undefined;
};


/**
 * Returns the end of the requested span.
 * @param {*} value Annotation.
 * @return {number|undefined} End of the span, or undefined if not attached.
 */
cvox.Spannable.prototype.getSpanEnd = function(value) {
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.value === value) {
      return span.end;
    }
  }
  return undefined;
};


/**
 * Returns a substring of this spannable.
 * Note that while similar to String#substring, this function is much less
 * permissive about its arguments. It does not accept arguments in the wrong
 * order or out of bounds.
 *
 * @param {number} start Start index, inclusive.
 * @param {number=} opt_end End index, exclusive.
 *     If excluded, the length of the string is used instead.
 * @return {!cvox.Spannable} Substring requested.
 */
cvox.Spannable.prototype.substring = function(start, opt_end) {
  var end = goog.isDef(opt_end) ? opt_end : this.string_.length;

  if (start < 0 || end > this.string_.length || start > end) {
    throw new RangeError('substring indices out of range');
  }

  var result = new cvox.Spannable(this.string_.substring(start, end));
  for (var i = 0; i < this.spans_.length; i++) {
    var span = this.spans_[i];
    if (span.start <= end && span.end >= start) {
      var newStart = Math.max(0, span.start - start);
      var newEnd = Math.min(end - start, span.end - start);
      result.spans_.push({ value: span.value, start: newStart, end: newEnd });
    }
  }
  return result;
};


/**
 * Trims whitespace from the beginning.
 * @return {!cvox.Spannable} String with whitespace removed.
 */
cvox.Spannable.prototype.trimLeft = function() {
  return this.trim_(true, false);
};


/**
 * Trims whitespace from the end.
 * @return {!cvox.Spannable} String with whitespace removed.
 */
cvox.Spannable.prototype.trimRight = function() {
  return this.trim_(false, true);
};


/**
 * Trims whitespace from the beginning and end.
 * @return {!cvox.Spannable} String with whitespace removed.
 */
cvox.Spannable.prototype.trim = function() {
  return this.trim_(true, true);
};


/**
 * Trims whitespace from either the beginning and end or both.
 * @param {boolean} trimStart Trims whitespace from the start of a string.
 * @param {boolean} trimEnd Trims whitespace from the end of a string.
 * @return {!cvox.Spannable} String with whitespace removed.
 * @private
 */
cvox.Spannable.prototype.trim_ = function(trimStart, trimEnd) {
  if (!trimStart && !trimEnd) {
    return this;
  }

  // Special-case whitespace-only strings, including the empty string.
  // As an arbitrary decision, we treat this as trimming the whitespace off the
  // end, rather than the beginning, of the string.
  // This choice affects which spans are kept.
  if (/^\s*$/.test(this.string_)) {
    return this.substring(0, 0);
  }

  // Otherwise, we have at least one non-whitespace character to use as an
  // anchor when trimming.
  var trimmedStart = trimStart ? this.string_.match(/^\s*/)[0].length : 0;
  var trimmedEnd = trimEnd ?
      this.string_.match(/\s*$/).index : this.string_.length;
  return this.substring(trimmedStart, trimmedEnd);
};


/**
 * Returns this spannable to a json serializable form, including the text and
 * span objects whose types have been registered with registerSerializableSpan
 * or registerStatelessSerializableSpan.
 * @return {!cvox.Spannable.SerializedSpannable_} the json serializable form.
 */
cvox.Spannable.prototype.toJson = function() {
  var result = {};
  result.string = this.string_;
  result.spans = [];
  for (var i = 0; i < this.spans_.length; ++i) {
    var span = this.spans_[i];
    // Use linear search, since using functions as property keys
    // is not reliable.
    var serializeInfo = goog.object.findValue(
        cvox.Spannable.serializableSpansByName_,
        function(v) { return v.ctor === span.value.constructor; });
    if (serializeInfo) {
      var spanObj = {type: serializeInfo.name,
                     start: span.start,
                     end: span.end};
      if (serializeInfo.toJson) {
        spanObj.value = serializeInfo.toJson.apply(span.value);
      }
      result.spans.push(spanObj);
    }
  }
  return result;
};


/**
 * Creates a spannable from a json serializable representation.
 * @param {!cvox.Spannable.SerializedSpannable_} obj object containing the
 *     serializable representation.
 * @return {!cvox.Spannable}
 */
cvox.Spannable.fromJson = function(obj) {
  if (typeof obj.string !== 'string') {
    throw 'Invalid spannable json object: string field not a string';
  }
  if (!(obj.spans instanceof Array)) {
    throw 'Invalid spannable json object: no spans array';
  }
  var result = new cvox.Spannable(obj.string);
  for (var i = 0, span; span = obj.spans[i]; ++i) {
    if (typeof span.type !== 'string') {
      throw 'Invalid span in spannable json object: type not a string';
    }
    if (typeof span.start !== 'number' || typeof span.end !== 'number') {
      throw 'Invalid span in spannable json object: start or end not a number';
    }
    var serializeInfo = cvox.Spannable.serializableSpansByName_[span.type];
    var value = serializeInfo.fromJson(span.value);
    result.setSpan(value, span.start, span.end);
  }
  return result;
};


/**
 * Registers a type that can be converted to a json serializable format.
 * @param {!Function} constructor The type of object that can be converted.
 * @param {string} name String identifier used in the serializable format.
 * @param {function(!Object): !Object} fromJson A function that converts
 *     the serializable object to an actual object of this type.
 * @param {function(!Object): !Object} toJson A function that converts
 *     this object to a json serializable object.  The function will
 *     be called with this set to the object to convert.
 */
cvox.Spannable.registerSerializableSpan = function(
    constructor, name, fromJson, toJson) {
  var obj = {name: name, ctor: constructor,
             fromJson: fromJson, toJson: toJson};
  cvox.Spannable.serializableSpansByName_[name] = obj;
};


/**
 * Registers an object type that can be converted to/from a json serializable
 * form.  Objects of this type carry no state that will be preserved
 * when serialized.
 * @param {!Function} constructor The type of the object that can be converted.
 *     This constructor will be called with no arguments to construct
 *     new objects.
 * @param {string} name Name of the type used in the serializable object.
 */
cvox.Spannable.registerStatelessSerializableSpan = function(
    constructor, name) {
  var obj = {name: name, ctor: constructor, toJson: undefined};
  /**
   * @param {!Object} obj
   * @return {!Object}
   */
  obj.fromJson = function(obj) {
     return new constructor();
  };
  cvox.Spannable.serializableSpansByName_[name] = obj;
};


/**
 * Describes how to convert a span type to/from serializable json.
 * @typedef {{ctor: !Function, name: string,
 *             fromJson: function(!Object): !Object,
 *             toJson: ((function(!Object): !Object)|undefined)}}
 * @private
 */
cvox.Spannable.SerializeInfo_;


/**
 * The serialized format of a spannable.
 * @typedef {{string: string, spans: Array.<cvox.Spannable.SerializedSpan_>}}
 * @private
 */
cvox.Spannable.SerializedSpannable_;


/**
 * The format of a single annotation in a serialized spannable.
 * @typedef {{type: string, value: !Object, start: number, end: number}}
 * @private
 */
cvox.Spannable.SerializedSpan_;

/**
 * Maps type names to serialization info objects.
 * @type {Object.<string, cvox.Spannable.SerializeInfo_>}
 * @private
 */
cvox.Spannable.serializableSpansByName_ = {};
