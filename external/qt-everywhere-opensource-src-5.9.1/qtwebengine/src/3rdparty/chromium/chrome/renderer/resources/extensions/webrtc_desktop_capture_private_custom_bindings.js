// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webrtcDesktopCapturePrivate API.

var binding = require('binding').Binding.create('webrtcDesktopCapturePrivate');
var sendRequest = require('sendRequest').sendRequest;
var idGenerator = requireNative('id_generator');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var pendingRequests = {};

  function onRequestResult(id, result) {
    if (id in pendingRequests) {
      var callback = pendingRequests[id];
      delete pendingRequests[id];
      callback(result);
    }
  }

  apiFunctions.setHandleRequest('chooseDesktopMedia',
                                function(sources, request, callback) {
    var id = idGenerator.GetNextId();
    pendingRequests[id] = callback;
    sendRequest(this.name,
                [id, sources, request, onRequestResult.bind(null, id)],
                this.definition.parameters);
    return id;
  });

  apiFunctions.setHandleRequest('cancelChooseDesktopMedia', function(id) {
    if (id in pendingRequests) {
      delete pendingRequests[id];
      sendRequest(this.name, [id], this.definition.parameters);
    }
  });
});

exports.$set('binding', binding.generate());
