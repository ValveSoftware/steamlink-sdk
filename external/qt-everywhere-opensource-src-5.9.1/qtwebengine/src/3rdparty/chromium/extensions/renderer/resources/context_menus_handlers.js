// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of custom bindings for the contextMenus API.
// This is used to implement the contextMenus API for extensions and for the
// <webview> tag (see chrome_web_view_experimental.js).

var contextMenuNatives = requireNative('context_menus');
var sendRequest = require('sendRequest').sendRequest;
var Event = require('event_bindings').Event;
var lastError = require('lastError');

// Add the bindings to the contextMenus API.
function createContextMenusHandlers(isWebview) {
  var eventName = isWebview ? 'webViewInternal.contextMenus' : 'contextMenus';
  // Some dummy value for chrome.contextMenus instances.
  // Webviews use positive integers, and 0 to denote an invalid webview ID.
  // The following constant is -1 to avoid any conflicts between webview IDs and
  // extensions.
  var INSTANCEID_NON_WEBVIEW = -1;

  // Generates a customCallback for a given method. |handleCallback| will be
  // invoked with |request.args| as parameters.
  function createCustomCallback(handleCallback) {
    return function(name, request, callback) {
      if (lastError.hasError(chrome)) {
        if (callback)
          callback();
        return;
      }
      var args = request.args;
      if (!isWebview) {
        // <webview>s have an extra item in front of the parameter list, which
        // specifies the viewInstanceId of the webview. This is used to hide
        // context menu events in one webview from another.
        // The non-webview chrome.contextMenus API is not called with such an
        // ID, so we prepend an ID to match the function signature.
        args = $Array.concat([INSTANCEID_NON_WEBVIEW], args);
      }
      $Function.apply(handleCallback, null, args);
      if (callback)
        callback();
    };
  }

  var contextMenus = {};
  contextMenus.handlers = {};
  contextMenus.event = new Event(eventName);

  contextMenus.getIdFromCreateProperties = function(createProperties) {
    if (typeof createProperties.id !== 'undefined')
      return createProperties.id;
    return createProperties.generatedId;
  };

  contextMenus.handlersForId = function(instanceId, id) {
    if (!contextMenus.handlers[instanceId]) {
      contextMenus.handlers[instanceId] = {
        generated: {},
        string: {}
      };
    }
    if (typeof id === 'number')
      return contextMenus.handlers[instanceId].generated;
    return contextMenus.handlers[instanceId].string;
  };

  contextMenus.ensureListenerSetup = function() {
    if (contextMenus.listening) {
      return;
    }
    contextMenus.listening = true;
    contextMenus.event.addListener(function(info) {
      var instanceId = INSTANCEID_NON_WEBVIEW;
      if (isWebview) {
        instanceId = info.webviewInstanceId;
        // Don't expose |webviewInstanceId| via the public API.
        delete info.webviewInstanceId;
      }

      var id = info.menuItemId;
      var onclick = contextMenus.handlersForId(instanceId, id)[id];
      if (onclick) {
        $Function.apply(onclick, null, arguments);
      }
    });
  };

  // To be used with apiFunctions.setHandleRequest
  var requestHandlers = { __proto__: null };
  // To be used with apiFunctions.setCustomCallback
  var callbacks = { __proto__: null };

  requestHandlers.create = function() {
    var createProperties = isWebview ? arguments[1] : arguments[0];
    createProperties.generatedId = contextMenuNatives.GetNextContextMenuId();
    var optArgs = {
      __proto__: null,
      customCallback: this.customCallback,
    };
    sendRequest(this.name, arguments, this.definition.parameters, optArgs);
    return contextMenus.getIdFromCreateProperties(createProperties);
  };

  callbacks.create =
      createCustomCallback(function(instanceId, createProperties) {
    var id = contextMenus.getIdFromCreateProperties(createProperties);
    var onclick = createProperties.onclick;
    if (onclick) {
      contextMenus.ensureListenerSetup();
      contextMenus.handlersForId(instanceId, id)[id] = onclick;
    }
  });

  callbacks.remove = createCustomCallback(function(instanceId, id) {
    delete contextMenus.handlersForId(instanceId, id)[id];
  });

  callbacks.update =
      createCustomCallback(function(instanceId, id, updateProperties) {
    var onclick = updateProperties.onclick;
    if (onclick) {
      contextMenus.ensureListenerSetup();
      contextMenus.handlersForId(instanceId, id)[id] = onclick;
    } else if (onclick === null) {
      // When onclick is explicitly set to null, remove the event listener.
      delete contextMenus.handlersForId(instanceId, id)[id];
    }
  });

  callbacks.removeAll = createCustomCallback(function(instanceId) {
    delete contextMenus.handlers[instanceId];
  });

  return {
    requestHandlers: requestHandlers,
    callbacks: callbacks
  };
}

exports.$set('create', createContextMenusHandlers);
