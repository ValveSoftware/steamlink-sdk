// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common test utilities.

/**
 * Allows console.log output.
 */
var showConsoleLogOutput = false;

/**
 * Conditionally allow console.log output based off of showConsoleLogOutput.
 */
console.log = function() {
  var originalConsoleLog = console.log;
  return function() {
    if (showConsoleLogOutput) {
      originalConsoleLog.apply(console, arguments);
    }
  };
}();

function emptyMock() {}

// Container for event handlers added by mocked 'addListener' functions.
var mockEventHandlers = {};

/**
 * Mocks 'addListener' function of an API event. The mocked function will keep
 * track of handlers.
 * @param {Object} topLevelContainer Top-level container of the original
 *     function. Can be either 'chrome' or 'instrumented'.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 */
function mockChromeEvent(topLevelContainer, eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var eventName = eventIdentifierParts.pop();
  var originalMethodContainer = topLevelContainer;
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    originalMethodContainer =
        originalMethodContainer[fragment] =
        originalMethodContainer[fragment] || {};
    mockEventContainer =
        mockEventContainer[fragment] =
        mockEventContainer[fragment] || {};
  });

  mockEventContainer[eventName] = [];
  originalMethodContainer[eventName] = {
    addListener: function(callback) {
      mockEventContainer[eventName].push(callback);
    }
  };
}

/**
 * Gets the array of event handlers added by a mocked 'addListener' function.
 * @param {string} eventIdentifier Event identifier, such as
 *     'runtime.onSuspend'.
 * @return {Array.<Function>} Array of handlers.
 */
function getMockHandlerContainer(eventIdentifier) {
  var eventIdentifierParts = eventIdentifier.split('.');
  var mockEventContainer = mockEventHandlers;
  eventIdentifierParts.forEach(function(fragment) {
    mockEventContainer = mockEventContainer[fragment];
  });

  return mockEventContainer;
}

/**
 * MockPromise
 * The JS test harness expects all calls to complete synchronously.
 * As a result, we can't use built-in JS promises since they run asynchronously.
 * Instead of mocking all possible calls to promises, a skeleton
 * implementation is provided to get the tests to pass.
 *
 * This functionality and logic originates from ECMAScript 6's spec of promises.
 */
var Promise = function() {
  function PromisePrototypeObject(asyncTask) {
    function isThenable(value) {
      return (typeof value === 'object') && isCallable(value.then);
    }

    function isCallable(value) {
      return typeof value === 'function';
    }

    function callResolveRejectFunc(func) {
      var funcResult;
      var funcResolved = false;
      func(
          function(resolveResult) {
            funcResult = resolveResult;
            funcResolved = true;
          },
          function(rejectResult) {
            funcResult = rejectResult;
            funcResolved = false;
          });
      return { result: funcResult, resolved: funcResolved };
    }

    function then(onResolve, onReject) {
      var resolutionHandler =
          isCallable(onResolve) ? onResolve : function() { return result; };
      var rejectionHandler =
          isCallable(onReject) ? onReject : function() { return result; };
      var handlerResult =
          resolved ? resolutionHandler(result) : rejectionHandler(result);
      var promiseResolved = resolved;
      if (isThenable(handlerResult)) {
        var resolveReject = callResolveRejectFunc(handlerResult.then);
        handlerResult = resolveReject.result;
        promiseResolved = resolveReject.resolved;
      }

      if (promiseResolved) {
        return Promise.resolve(handlerResult);
      } else {
        return Promise.reject(handlerResult);
      }
    }

    // Promises use the function name "catch" to call back error handlers.
    // We can't use "catch" since function or variable names cannot use the word
    // "catch".
    function catchFunc(onRejected) {
      return this.then(undefined, onRejected);
    }

    var resolveReject = callResolveRejectFunc(asyncTask);
    var result = resolveReject.result;
    var resolved = resolveReject.resolved;

    if (isThenable(result)) {
      var thenResolveReject = callResolveRejectFunc(result.then);
      result = thenResolveReject.result;
      resolved = thenResolveReject.resolved;
    }

    return {then: then, catch: catchFunc, isPromise: true};
  }

  function all(arrayOfPromises) {
    var results = [];
    for (i = 0; i < arrayOfPromises.length; i++) {
      if (arrayOfPromises[i].isPromise) {
        arrayOfPromises[i].then(function(result) {
          results[i] = result;
        });
      } else {
        results[i] = arrayOfPromises[i];
      }
    }
    var promise = new PromisePrototypeObject(function(resolve) {
      resolve(results);
    });
    return promise;
  }

  function resolve(value) {
    var promise = new PromisePrototypeObject(function(resolve) {
      resolve(value);
    });
    return promise;
  }

  function reject(value) {
    var promise = new PromisePrototypeObject(function(resolve, reject) {
      reject(value);
    });
    return promise;
  }

  PromisePrototypeObject.all = all;
  PromisePrototypeObject.resolve = resolve;
  PromisePrototypeObject.reject = reject;
  return PromisePrototypeObject;
}();

/**
 * Sets up the test to expect a Chrome Local Storage call.
 * @param {Object} fixture Mock JS Test Object.
 * @param {Object} defaultObject Storage request default object.
 * @param {Object} result Storage result.
 * @param {boolean=} opt_AllowRejection Allow Promise Rejection
 */
function expectChromeLocalStorageGet(
    fixture, defaultObject, result, opt_AllowRejection) {
  if (opt_AllowRejection === undefined) {
    fixture.mockApis.expects(once()).
      fillFromChromeLocalStorage(eqJSON(defaultObject)).
      will(returnValue(Promise.resolve(result)));
  } else {
    fixture.mockApis.expects(once()).
      fillFromChromeLocalStorage(eqJSON(defaultObject), opt_AllowRejection).
      will(returnValue(Promise.resolve(result)));
  }
}
