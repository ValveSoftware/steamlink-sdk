// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Event = require('event_bindings').Event;
var sendRequest = require('sendRequest').sendRequest;
var validate = require('schemaUtils').validate;

function extendSchema(schema) {
  var extendedSchema = $Array.slice(schema);
  extendedSchema.unshift({'type': 'string'});
  return extendedSchema;
}

function ChromeSetting(prefKey, valueSchema) {
  this.get = function(details, callback) {
    var getSchema = this.functionSchemas.get.definition.parameters;
    validate([details, callback], getSchema);
    return sendRequest('types.ChromeSetting.get',
                       [prefKey, details, callback],
                       extendSchema(getSchema));
  };
  this.set = function(details, callback) {
    var setSchema = $Array.slice(
        this.functionSchemas.set.definition.parameters);
    setSchema[0].properties.value = valueSchema;
    validate([details, callback], setSchema);
    return sendRequest('types.ChromeSetting.set',
                       [prefKey, details, callback],
                       extendSchema(setSchema));
  };
  this.clear = function(details, callback) {
    var clearSchema = this.functionSchemas.clear.definition.parameters;
    validate([details, callback], clearSchema);
    return sendRequest('types.ChromeSetting.clear',
                       [prefKey, details, callback],
                       extendSchema(clearSchema));
  };
  this.onChange = new Event('types.ChromeSetting.' + prefKey + '.onChange');
};

exports.ChromeSetting = ChromeSetting;
