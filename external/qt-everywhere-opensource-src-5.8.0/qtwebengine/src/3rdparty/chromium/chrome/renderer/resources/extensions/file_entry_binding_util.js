// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystemNatives = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemNatives.GetIsolatedFileSystem;
var sendRequest = require('sendRequest');
var lastError = require('lastError');
var GetModuleSystem = requireNative('v8_context').GetModuleSystem;
// TODO(sammc): Don't require extension. See http://crbug.com/235689.
var GetExtensionViews = requireNative('runtime').GetExtensionViews;

// For a given |apiName|, generates object with two elements that are used
// in file system relayed APIs:
// * 'bindFileEntryCallback' function that provides mapping between JS objects
//   into actual FileEntry|DirectoryEntry objects.
// * 'entryIdManager' object that implements methods for keeping the tracks of
//   previously saved file entries.
function getFileBindingsForApi(apiName) {
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
          function(name, request, callback, response) {
        if (callback) {
          if (!response) {
            callback();
            return;
          }

          var entries = [];
          var hasError = false;

          var getEntryError = function(fileError) {
            if (!hasError) {
              hasError = true;
              lastError.run(
                  apiName + '.' + functionName,
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
                        apiName + '.' + functionName, request, callback,
                        [entries]);
                  } else {
                    sendRequest.safeCallbackApply(
                        apiName + '.' + functionName, request, callback,
                        [entries[0]]);
                  }
                }
              }
              // TODO(koz): fs.root.getFile() makes a trip to the browser
              // process, but it might be possible avoid that by calling
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
                lastError.run(apiName + '.' + functionName,
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
        apiName).bindFileEntryCallback;
    var entryIdManager = backgroundPageModuleSystem.require('entryIdManager');
  }
  return {bindFileEntryCallback: bindFileEntryCallback,
          entryIdManager: entryIdManager};
}

exports.$set('getFileBindingsForApi', getFileBindingsForApi);
