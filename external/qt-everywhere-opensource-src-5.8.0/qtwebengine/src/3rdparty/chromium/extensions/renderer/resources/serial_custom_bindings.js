// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the Serial API.
 *
 * The bindings are implemented by asynchronously delegating to the
 * serial_service module. The functions that apply to a particular connection
 * are delegated to the appropriate method on the Connection object specified by
 * the ID parameter.
 */

var binding = require('binding').Binding.create('serial');
var context = requireNative('v8_context');
var eventBindings = require('event_bindings');
var utils = require('utils');

var serialServicePromise = function() {
  // getBackgroundPage is not available in unit tests so fall back to the
  // current page's serial_service module.
  if (!chrome.runtime.getBackgroundPage)
    return requireAsync('serial_service');

  // Load the serial_service module from the background page if one exists. This
  // is necessary for serial connections created in one window to be usable
  // after that window is closed. This is necessary because the Mojo async
  // waiter only functions while the v8 context remains.
  return utils.promise(chrome.runtime.getBackgroundPage).then(function(bgPage) {
    return context.GetModuleSystem(bgPage).requireAsync('serial_service');
  }).catch(function() {
    return requireAsync('serial_service');
  });
}();

function forwardToConnection(methodName) {
  return function(connectionId) {
    var args = $Array.slice(arguments, 1);
    return serialServicePromise.then(function(serialService) {
      return serialService.getConnection(connectionId);
    }).then(function(connection) {
      return $Function.apply(connection[methodName], connection, args);
    });
  };
}

function addEventListeners(connection, id) {
  connection.onData = function(data) {
    eventBindings.dispatchEvent(
        'serial.onReceive', [{connectionId: id, data: data}]);
  };
  connection.onError = function(error) {
    eventBindings.dispatchEvent(
        'serial.onReceiveError', [{connectionId: id, error: error}]);
  };
}

serialServicePromise.then(function(serialService) {
  return serialService.getConnections().then(function(connections) {
    for (var entry of connections) {
      var connection = entry[1];
      addEventListeners(connection, entry[0]);
      connection.resumeReceives();
    };
  });
});

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequestWithPromise('getDevices', function() {
    return serialServicePromise.then(function(serialService) {
      return serialService.getDevices();
    });
  });

  apiFunctions.setHandleRequestWithPromise('connect', function(path, options) {
    return serialServicePromise.then(function(serialService) {
      return serialService.createConnection(path, options);
    }).then(function(result) {
      addEventListeners(result.connection, result.info.connectionId);
      return result.info;
    }).catch (function(e) {
      throw new Error('Failed to connect to the port.');
    });
  });

  apiFunctions.setHandleRequestWithPromise(
      'disconnect', forwardToConnection('close'));
  apiFunctions.setHandleRequestWithPromise(
      'getInfo', forwardToConnection('getInfo'));
  apiFunctions.setHandleRequestWithPromise(
      'update', forwardToConnection('setOptions'));
  apiFunctions.setHandleRequestWithPromise(
      'getControlSignals', forwardToConnection('getControlSignals'));
  apiFunctions.setHandleRequestWithPromise(
      'setControlSignals', forwardToConnection('setControlSignals'));
  apiFunctions.setHandleRequestWithPromise(
      'flush', forwardToConnection('flush'));
  apiFunctions.setHandleRequestWithPromise(
      'setPaused', forwardToConnection('setPaused'));
  apiFunctions.setHandleRequestWithPromise(
      'send', forwardToConnection('send'));

  apiFunctions.setHandleRequestWithPromise('getConnections', function() {
    return serialServicePromise.then(function(serialService) {
      return serialService.getConnections();
    }).then(function(connections) {
      var promises = [];
      for (var connection of connections.values()) {
        promises.push(connection.getInfo());
      }
      return Promise.all(promises);
    });
  });
});

exports.$set('binding', binding.generate());
