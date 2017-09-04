// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the logPrivate API.
var binding = require('binding').Binding.create('logPrivate');
var sendRequest = require('sendRequest');

var getFileBindingsForApi =
    require('fileEntryBindingUtil').getFileBindingsForApi;
var fileBindings = getFileBindingsForApi('logPrivate');
var bindFileEntryCallback = fileBindings.bindFileEntryCallback;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var fileSystem = bindingsAPI.compiledApi;

  $Array.forEach(['dumpLogs'],
                  function(functionName) {
    bindFileEntryCallback(functionName, apiFunctions);
  });

});

exports.$set('bindFileEntryCallback', bindFileEntryCallback);
exports.$set('binding', binding.generate());
