// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = require('binding').Binding.create('chromeWebViewInternal');
var contextMenusHandlers = require('contextMenusHandlers');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  var handlers = contextMenusHandlers.create(true /* isWebview */);

  apiFunctions.setHandleRequest('contextMenusCreate',
      handlers.requestHandlers.create);

  apiFunctions.setCustomCallback('contextMenusCreate',
      handlers.callbacks.create);

  apiFunctions.setCustomCallback('contextMenusUpdate',
      handlers.callbacks.update);

  apiFunctions.setCustomCallback('contextMenusRemove',
      handlers.callbacks.remove);

  apiFunctions.setCustomCallback('contextMenusRemoveAll',
      handlers.callbacks.removeAll);

});

exports.$set('ChromeWebView', binding.generate());
