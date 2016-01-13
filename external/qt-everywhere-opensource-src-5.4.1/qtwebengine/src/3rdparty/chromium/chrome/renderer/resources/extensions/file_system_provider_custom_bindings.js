// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystemProvider API.

var binding = require('binding').Binding.create('fileSystemProvider');
var fileSystemProviderInternal =
    require('binding').Binding.create('fileSystemProviderInternal').generate();
var eventBindings = require('event_bindings');
var fileSystemNatives = requireNative('file_system_natives');
var GetDOMError = fileSystemNatives.GetDOMError;

/**
 * Annotates a date with its serialized value.
 * @param {Date} date Input date.
 * @return {Date} Date with an extra <code>value</code> attribute.
 */
function annotateDate(date) {
  // Copy in case the input date is frozen.
  var result = new Date(date.getTime());
  result.value = result.toString();
  return result;
}

/**
 * Annotates an entry metadata by serializing its modifiedTime value.
 * @param {EntryMetadata} metadata Input metadata.
 * @return {EntryMetadata} metadata Annotated metadata, which can be passed
 *     back to the C++ layer.
 */
function annotateMetadata(metadata) {
  var result = {
    isDirectory: metadata.isDirectory,
    name: metadata.name,
    size: metadata.size,
    modificationTime: annotateDate(metadata.modificationTime)
  };
  return result;
}

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setUpdateArgumentsPostValidate(
    'mount',
    function(options, successCallback, errorCallback) {
      // Piggyback the error callback onto the success callback,
      // so we can use the error callback later.
      successCallback.errorCallback_ = errorCallback;
      return [options, successCallback];
    });

  apiFunctions.setCustomCallback(
    'mount',
    function(name, request, response) {
      var domError = null;
      if (request.callback && response) {
        // DOMError is present only if mount() failed.
        if (response[0]) {
          // Convert a Dictionary to a DOMError.
          domError = GetDOMError(response[0].name, response[0].message);
          response.length = 1;
        }

        var successCallback = request.callback;
        var errorCallback = request.callback.errorCallback_;
        delete request.callback;

        if (domError)
          errorCallback(domError);
        else
          successCallback();
      }
    });

  apiFunctions.setUpdateArgumentsPostValidate(
    'unmount',
    function(options, successCallback, errorCallback) {
      // Piggyback the error callback onto the success callback,
      // so we can use the error callback later.
      successCallback.errorCallback_ = errorCallback;
      return [options, successCallback];
    });

  apiFunctions.setCustomCallback(
    'unmount',
    function(name, request, response) {
      var domError = null;
      if (request.callback) {
        // DOMError is present only if mount() failed.
        if (response && response[0]) {
          // Convert a Dictionary to a DOMError.
          domError = GetDOMError(response[0].name, response[0].message);
          response.length = 1;
        }

        var successCallback = request.callback;
        var errorCallback = request.callback.errorCallback_;
        delete request.callback;

        if (domError)
          errorCallback(domError);
        else
          successCallback();
      }
    });
});

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onUnmountRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function() {
        fileSystemProviderInternal.unmountRequestedSuccess(
            options.fileSystemId, options.requestId);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.unmountRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onGetMetadataRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function(metadata) {
        fileSystemProviderInternal.getMetadataRequestedSuccess(
            options.fileSystemId,
            options.requestId,
            annotateMetadata(metadata));
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.getMetadataRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onReadDirectoryRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function(entries, hasNext) {
        var annotatedEntries = entries.map(annotateMetadata);
        fileSystemProviderInternal.readDirectoryRequestedSuccess(
            options.fileSystemId, options.requestId, annotatedEntries, hasNext);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.readDirectoryRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onOpenFileRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function() {
        fileSystemProviderInternal.openFileRequestedSuccess(
            options.fileSystemId, options.requestId);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.openFileRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onCloseFileRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function() {
        fileSystemProviderInternal.closeFileRequestedSuccess(
            options.fileSystemId, options.requestId);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.closeFileRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onReadFileRequested',
    function(args, dispatch) {
      var options = args[0];
      var onSuccessCallback = function(data, hasNext) {
        fileSystemProviderInternal.readFileRequestedSuccess(
            options.fileSystemId, options.requestId, data, hasNext);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.readFileRequestedError(
            options.fileSystemId, options.requestId, error);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

exports.binding = binding.generate();
