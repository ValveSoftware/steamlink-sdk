// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the developerPrivate API.

var binding = require('binding').Binding.create('developerPrivate');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // Converts the argument of |functionName| from DirectoryEntry to URL.
  function bindFileSystemFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(directoryEntry, callback) {
          var fileSystemName = directoryEntry.filesystem.name;
          var relativePath = $String.slice(directoryEntry.fullPath, 1);
          var url = directoryEntry.toURL();
          return [fileSystemName, relativePath, url, callback];
    });
  }

  bindFileSystemFunction('loadDirectory');

});

exports.binding = binding.generate();
