// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the contentSettings API.

var sendRequest = require('sendRequest').sendRequest;
var validate = require('schemaUtils').validate;

function extendSchema(schema) {
  var extendedSchema = $Array.slice(schema);
  extendedSchema.unshift({'type': 'string'});
  return extendedSchema;
}

function ContentSetting(contentType, settingSchema) {
  this.get = function(details, callback) {
    var getSchema = this.functionSchemas.get.definition.parameters;
    validate([details, callback], getSchema);
    return sendRequest('contentSettings.get',
                       [contentType, details, callback],
                       extendSchema(getSchema));
  };
  this.set = function(details, callback) {
    var setSchema = $Array.slice(
        this.functionSchemas.set.definition.parameters);
    setSchema[0].properties.setting = settingSchema;
    validate([details, callback], setSchema);
    return sendRequest('contentSettings.set',
                       [contentType, details, callback],
                       extendSchema(setSchema));
  };
  this.clear = function(details, callback) {
    var clearSchema = this.functionSchemas.clear.definition.parameters;
    validate([details, callback], clearSchema);
    return sendRequest('contentSettings.clear',
                       [contentType, details, callback],
                       extendSchema(clearSchema));
  };
  this.getResourceIdentifiers = function(callback) {
    var schema =
        this.functionSchemas.getResourceIdentifiers.definition.parameters;
    validate([callback], schema);
    return sendRequest(
        'contentSettings.getResourceIdentifiers',
        [contentType, callback],
        extendSchema(schema));
  };
}

exports.ContentSetting = ContentSetting;
