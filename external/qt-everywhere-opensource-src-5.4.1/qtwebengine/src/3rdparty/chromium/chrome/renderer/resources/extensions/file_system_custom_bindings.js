// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystem API.

var binding = require('binding').Binding.create('fileSystem');

var fileSystemNatives = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemNatives.GetIsolatedFileSystem;
var lastError = require('lastError');
var sendRequest = require('sendRequest');
var GetModuleSystem = requireNative('v8_context').GetModuleSystem;
// TODO(sammc): Don't require extension. See http://crbug.com/235689.
var GetExtensionViews = requireNative('runtime').GetExtensionViews;

// Fallback to using the current window if no background page is running.
var backgroundPage = GetExtensionViews(-1, 'BACKGROUND')[0] || window;
var backgroundPageModuleSystem = GetModuleSystem(backgroundPage);

// All windows use the bindFileEntryCallback from the background page so their
// FileEntry objects have the background page's context as their own. This
// allows them to be used from other windows (including the background page)
// after the original window is closed.
if (window == backgroundPage) {
  var bindFileEntryCallback = function(functionName, apiFunctions) {
    apiFunctions.setCustomCallback(functionName,
        function(name, request, response) {
      if (request.callback && response) {
        var callback = request.callback;
        request.callback = null;

        var entries = [];
        var hasError = false;

        var getEntryError = function(fileError) {
          if (!hasError) {
            hasError = true;
            lastError.run(
                'fileSystem.' + functionName,
                'Error getting fileEntry, code: ' + fileError.code,
                request.stack,
                callback);
          }
        }

        // Loop through the response entries and asynchronously get the
        // FileEntry for each. We use hasError to ensure that only the first
        // error is reported. Note that an error can occur either during the
        // loop or in the asynchronous error callback to getFile.
        $Array.forEach(response.entries, function(entry) {
          if (hasError)
            return;
          var fileSystemId = entry.fileSystemId;
          var baseName = entry.baseName;
          var id = entry.id;
          var fs = GetIsolatedFileSystem(fileSystemId);

          try {
            var getEntryCallback = function(fileEntry) {
              if (hasError)
                return;
              entryIdManager.registerEntry(id, fileEntry);
              entries.push(fileEntry);
              // Once all entries are ready, pass them to the callback. In the
              // event of an error, this condition will never be satisfied so
              // the callback will not be called with any entries.
              if (entries.length == response.entries.length) {
                if (response.multiple) {
                  sendRequest.safeCallbackApply(
                      'fileSystem.' + functionName, request, callback,
                      [entries]);
                } else {
                  sendRequest.safeCallbackApply(
                      'fileSystem.' + functionName, request, callback,
                      [entries[0]]);
                }
              }
            }
            // TODO(koz): fs.root.getFile() makes a trip to the browser process,
            // but it might be possible avoid that by calling
            // WebDOMFileSystem::createV8Entry().
            if (entry.isDirectory) {
              fs.root.getDirectory(baseName, {}, getEntryCallback,
                                   getEntryError);
            } else {
              fs.root.getFile(baseName, {}, getEntryCallback, getEntryError);
            }
          } catch (e) {
            if (!hasError) {
              hasError = true;
              lastError.run('fileSystem.' + functionName,
                            'Error getting fileEntry: ' + e.stack,
                            request.stack,
                            callback);
            }
          }
        });
      }
    });
  };
  var entryIdManager = require('entryIdManager');
} else {
  // Force the fileSystem API to be loaded in the background page. Using
  // backgroundPageModuleSystem.require('fileSystem') is insufficient as
  // requireNative is only allowed while lazily loading an API.
  backgroundPage.chrome.fileSystem;
  var bindFileEntryCallback = backgroundPageModuleSystem.require(
      'fileSystem').bindFileEntryCallback;
  var entryIdManager = backgroundPageModuleSystem.require('entryIdManager');
}

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var fileSystem = bindingsAPI.compiledApi;

  function bindFileEntryFunction(functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(fileEntry, callback) {
      var fileSystemName = fileEntry.filesystem.name;
      var relativePath = $String.slice(fileEntry.fullPath, 1);
      return [fileSystemName, relativePath, callback];
    });
  }
  $Array.forEach(['getDisplayPath', 'getWritableEntry', 'isWritableEntry'],
                  bindFileEntryFunction);

  $Array.forEach(['getWritableEntry', 'chooseEntry', 'restoreEntry'],
                  function(functionName) {
    bindFileEntryCallback(functionName, apiFunctions);
  });

  apiFunctions.setHandleRequest('retainEntry', function(fileEntry) {
    var id = entryIdManager.getEntryId(fileEntry);
    if (!id)
      return '';
    var fileSystemName = fileEntry.filesystem.name;
    var relativePath = $String.slice(fileEntry.fullPath, 1);

    sendRequest.sendRequest(this.name, [id, fileSystemName, relativePath],
      this.definition.parameters, {});
    return id;
  });

  apiFunctions.setHandleRequest('isRestorable',
      function(id, callback) {
    var savedEntry = entryIdManager.getEntryById(id);
    if (savedEntry) {
      sendRequest.safeCallbackApply(
          'fileSystem.isRestorable',
          {'stack': sendRequest.getExtensionStackTrace()},
          callback,
          [true]);
    } else {
      sendRequest.sendRequest(
          this.name, [id, callback], this.definition.parameters, {});
    }
  });

  apiFunctions.setUpdateArgumentsPostValidate('restoreEntry',
      function(id, callback) {
    var savedEntry = entryIdManager.getEntryById(id);
    if (savedEntry) {
      // We already have a file entry for this id so pass it to the callback and
      // send a request to the browser to move it to the back of the LRU.
      sendRequest.safeCallbackApply(
          'fileSystem.restoreEntry',
          {'stack': sendRequest.getExtensionStackTrace()},
          callback,
          [savedEntry]);
      return [id, false, null];
    } else {
      // Ask the browser process for a new file entry for this id, to be passed
      // to |callback|.
      return [id, true, callback];
    }
  });

  // TODO(benwells): Remove these deprecated versions of the functions.
  fileSystem.getWritableFileEntry = function() {
    console.log("chrome.fileSystem.getWritableFileEntry is deprecated");
    console.log("Please use chrome.fileSystem.getWritableEntry instead");
    $Function.apply(fileSystem.getWritableEntry, this, arguments);
  };

  fileSystem.isWritableFileEntry = function() {
    console.log("chrome.fileSystem.isWritableFileEntry is deprecated");
    console.log("Please use chrome.fileSystem.isWritableEntry instead");
    $Function.apply(fileSystem.isWritableEntry, this, arguments);
  };

  fileSystem.chooseFile = function() {
    console.log("chrome.fileSystem.chooseFile is deprecated");
    console.log("Please use chrome.fileSystem.chooseEntry instead");
    $Function.apply(fileSystem.chooseEntry, this, arguments);
  };
});

exports.bindFileEntryCallback = bindFileEntryCallback;
exports.binding = binding.generate();
