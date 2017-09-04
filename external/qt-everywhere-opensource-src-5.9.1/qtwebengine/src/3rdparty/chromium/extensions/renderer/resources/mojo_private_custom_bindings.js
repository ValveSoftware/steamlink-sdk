// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the mojoPrivate API.
 */

let binding = require('binding').Binding.create('mojoPrivate');

binding.registerCustomHook(function(bindingsAPI) {
  let apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('define', function(name, deps, factory) {
    define(name, deps || [], factory);
  });

  apiFunctions.setHandleRequest('requireAsync', function(moduleName) {
    return requireAsync(moduleName);
  });
});

exports.$set('binding', binding.generate());
