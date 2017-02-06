// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the runtime API.

var binding = require('binding').Binding.create('runtime');

var messaging = require('messaging');
var runtimeNatives = requireNative('runtime');
var process = requireNative('process');
var utils = require('utils');

var WINDOW = {};
try {
  WINDOW = window;
} catch (e) {
  // Running in SW context.
  // TODO(lazyboy): Synchronous access to background page is not possible from
  // service worker context. Decide what we should do in this case for the class
  // of APIs that require access to background page or window object
}

var backgroundPage = WINDOW;
var backgroundRequire = require;
var contextType = process.GetContextType();

if (contextType == 'BLESSED_EXTENSION' ||
    contextType == 'UNBLESSED_EXTENSION') {
  var manifest = runtimeNatives.GetManifest();
  if (manifest.app && manifest.app.background) {
    // Get the background page if one exists. Otherwise, default to the current
    // window.
    backgroundPage = runtimeNatives.GetExtensionViews(-1, 'BACKGROUND')[0];
    if (backgroundPage) {
      var GetModuleSystem = requireNative('v8_context').GetModuleSystem;
      backgroundRequire = GetModuleSystem(backgroundPage).require;
    } else {
      backgroundPage = WINDOW;
    }
  }
}

// For packaged apps, all windows use the bindFileEntryCallback from the
// background page so their FileEntry objects have the background page's context
// as their own.  This allows them to be used from other windows (including the
// background page) after the original window is closed.
if (WINDOW == backgroundPage) {
  var lastError = require('lastError');
  var fileSystemNatives = requireNative('file_system_natives');
  var GetIsolatedFileSystem = fileSystemNatives.GetIsolatedFileSystem;
  var bindDirectoryEntryCallback = function(functionName, apiFunctions) {
    apiFunctions.setCustomCallback(functionName,
        function(name, request, callback, response) {
      if (callback) {
        if (!response) {
          callback();
          return;
        }
        var fileSystemId = response.fileSystemId;
        var baseName = response.baseName;
        var fs = GetIsolatedFileSystem(fileSystemId);

        try {
          fs.root.getDirectory(baseName, {}, callback, function(fileError) {
            lastError.run('runtime.' + functionName,
                          'Error getting Entry, code: ' + fileError.code,
                          request.stack,
                          callback);
          });
        } catch (e) {
          lastError.run('runtime.' + functionName,
                        'Error: ' + e.stack,
                        request.stack,
                        callback);
        }
      }
    });
  };
} else {
  // Force the runtime API to be loaded in the background page. Using
  // backgroundPageModuleSystem.require('runtime') is insufficient as
  // requireNative is only allowed while lazily loading an API.
  backgroundPage.chrome.runtime;
  var bindDirectoryEntryCallback =
      backgroundRequire('runtime').bindDirectoryEntryCallback;
}

binding.registerCustomHook(function(binding, id, contextType) {
  var apiFunctions = binding.apiFunctions;
  var runtime = binding.compiledApi;

  //
  // Unprivileged APIs.
  //

  if (id != '')
    utils.defineProperty(runtime, 'id', id);

  apiFunctions.setHandleRequest('getManifest', function() {
    return runtimeNatives.GetManifest();
  });

  apiFunctions.setHandleRequest('getURL', function(path) {
    path = $String.self(path);
    if (!path.length || path[0] != '/')
      path = '/' + path;
    return 'chrome-extension://' + id + path;
  });

  var sendMessageUpdateArguments = messaging.sendMessageUpdateArguments;
  apiFunctions.setUpdateArgumentsPreValidate(
      'sendMessage',
      $Function.bind(sendMessageUpdateArguments, null, 'sendMessage',
                     true /* hasOptionsArgument */));
  apiFunctions.setUpdateArgumentsPreValidate(
      'sendNativeMessage',
      $Function.bind(sendMessageUpdateArguments, null, 'sendNativeMessage',
                     false /* hasOptionsArgument */));

  apiFunctions.setHandleRequest(
      'sendMessage',
      function(targetId, message, options, responseCallback) {
    var connectOptions = $Object.assign({
      __proto__: null,
      name: messaging.kMessageChannel,
    }, options);
    var port = runtime.connect(targetId, connectOptions);
    messaging.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('sendNativeMessage',
                                function(targetId, message, responseCallback) {
    var port = runtime.connectNative(targetId);
    messaging.sendMessageImpl(port, message, responseCallback);
  });

  apiFunctions.setHandleRequest('connect', function(targetId, connectInfo) {
    if (!targetId) {
      // id is only defined inside extensions. If we're in a webpage, the best
      // we can do at this point is to fail.
      if (!id) {
        throw new Error('chrome.runtime.connect() called from a webpage must ' +
                        'specify an Extension ID (string) for its first ' +
                        'argument');
      }
      targetId = id;
    }

    var name = '';
    if (connectInfo && connectInfo.name)
      name = connectInfo.name;

    var includeTlsChannelId =
      !!(connectInfo && connectInfo.includeTlsChannelId);

    var portId = runtimeNatives.OpenChannelToExtension(targetId, name,
                                                       includeTlsChannelId);
    if (portId >= 0)
      return messaging.createPort(portId, name);
  });

  //
  // Privileged APIs.
  //
  if (contextType != 'BLESSED_EXTENSION')
    return;

  apiFunctions.setHandleRequest('connectNative',
                                function(nativeAppName) {
    var portId = runtimeNatives.OpenChannelToNativeApp(nativeAppName);
    if (portId >= 0)
      return messaging.createPort(portId, '');
    throw new Error('Error connecting to native app: ' + nativeAppName);
  });

  apiFunctions.setCustomCallback('getBackgroundPage',
                                 function(name, request, callback, response) {
    if (callback) {
      var bg = runtimeNatives.GetExtensionViews(-1, 'BACKGROUND')[0] || null;
      callback(bg);
    }
  });

  bindDirectoryEntryCallback('getPackageDirectoryEntry', apiFunctions);
});

exports.$set('bindDirectoryEntryCallback', bindDirectoryEntryCallback);
exports.$set('binding', binding.generate());
