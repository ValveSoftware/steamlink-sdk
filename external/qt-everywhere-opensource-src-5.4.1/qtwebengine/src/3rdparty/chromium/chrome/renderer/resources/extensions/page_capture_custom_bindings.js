// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the pageCapture API.

var binding = require('binding').Binding.create('pageCapture');

var pageCaptureNatives = requireNative('page_capture');
var CreateBlob = pageCaptureNatives.CreateBlob;
var SendResponseAck = pageCaptureNatives.SendResponseAck;

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback('saveAsMHTML',
                                 function(name, request, response) {
    var path = response.mhtmlFilePath;
    var size = response.mhtmlFileLength;

    if (request.callback)
      request.callback(CreateBlob(path, size));
    request.callback = null;

    // Notify the browser. Now that the blob is referenced from JavaScript,
    // the browser can drop its reference to it.
    SendResponseAck(request.id);
  });
});

exports.binding = binding.generate();
