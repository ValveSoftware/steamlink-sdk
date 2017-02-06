// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the systemIndicator API.
// TODO(dewittj) Refactor custom binding to reduce redundancy between the
// extension action APIs.

var binding = require('binding').Binding.create('systemIndicator');

var setIcon = require('setIcon').setIcon;
var sendRequest = require('sendRequest').sendRequest;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('setIcon', function(details, callback) {
    setIcon(details, function(args) {
      sendRequest(this.name, [args, callback], this.definition.parameters);
    }.bind(this));
  });
});

exports.$set('binding', binding.generate());
