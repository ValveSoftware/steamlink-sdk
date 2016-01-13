// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview Utility objects and functions for Google Now extension.
 * Most important entities here:
 * (1) 'wrapper' is a module used to add error handling and other services to
 *     callbacks for HTML and Chrome functions and Chrome event listeners.
 *     Chrome invokes extension code through event listeners. Once entered via
 *     an event listener, the extension may call a Chrome/HTML API method
 *     passing a callback (and so forth), and that callback must occur later,
 *     otherwise, we generate an error. Chrome may unload event pages waiting
 *     for an event. When the event fires, Chrome will reload the event page. We
 *     don't require event listeners to fire because they are generally not
 *     predictable (like a button clicked event).
 * (2) Task Manager (built with buildTaskManager() call) provides controlling
 *     mutually excluding chains of callbacks called tasks. Task Manager uses
 *     WrapperPlugins to add instrumentation code to 'wrapper' to determine
 *     when a task completes.
 */

// TODO(vadimt): Use server name in the manifest.

/**
 * Notification server URL.
 */
var NOTIFICATION_CARDS_URL = 'https://www.googleapis.com/chromenow/v1';

/**
 * Returns true if debug mode is enabled.
 * localStorage returns items as strings, which means if we store a boolean,
 * it returns a string. Use this function to compare against true.
 * @return {boolean} Whether debug mode is enabled.
 */
function isInDebugMode() {
  return localStorage.debug_mode === 'true';
}

/**
 * Initializes for debug or release modes of operation.
 */
function initializeDebug() {
  if (isInDebugMode()) {
    NOTIFICATION_CARDS_URL =
        localStorage['server_url'] || NOTIFICATION_CARDS_URL;
  }
}

initializeDebug();

/**
 * Conditionally allow console.log output based off of the debug mode.
 */
console.log = function() {
  var originalConsoleLog = console.log;
  return function() {
    if (isInDebugMode()) {
      originalConsoleLog.apply(console, arguments);
    }
  };
}();

/**
 * Explanation Card Storage.
 */
if (localStorage['explanatoryCardsShown'] === undefined)
  localStorage['explanatoryCardsShown'] = 0;

/**
 * Location Card Count Cleanup.
 */
if (localStorage.locationCardsShown !== undefined)
  localStorage.removeItem('locationCardsShown');

/**
 * Builds an error object with a message that may be sent to the server.
 * @param {string} message Error message. This message may be sent to the
 *     server.
 * @return {Error} Error object.
 */
function buildErrorWithMessageForServer(message) {
  var error = new Error(message);
  error.canSendMessageToServer = true;
  return error;
}

/**
 * Checks for internal errors.
 * @param {boolean} condition Condition that must be true.
 * @param {string} message Diagnostic message for the case when the condition is
 *     false.
 */
function verify(condition, message) {
  if (!condition)
    throw buildErrorWithMessageForServer('ASSERT: ' + message);
}

/**
 * Builds a request to the notification server.
 * @param {string} method Request method.
 * @param {string} handlerName Server handler to send the request to.
 * @param {string=} opt_contentType Value for the Content-type header.
 * @return {XMLHttpRequest} Server request.
 */
function buildServerRequest(method, handlerName, opt_contentType) {
  var request = new XMLHttpRequest();

  request.responseType = 'text';
  request.open(method, NOTIFICATION_CARDS_URL + '/' + handlerName, true);
  if (opt_contentType)
    request.setRequestHeader('Content-type', opt_contentType);

  return request;
}

/**
 * Sends an error report to the server.
 * @param {Error} error Error to send.
 */
function sendErrorReport(error) {
  // Don't remove 'error.stack.replace' below!
  var filteredStack = error.canSendMessageToServer ?
      error.stack : error.stack.replace(/.*\n/, '(message removed)\n');
  var file;
  var line;
  var topFrameLineMatch = filteredStack.match(/\n    at .*\n/);
  var topFrame = topFrameLineMatch && topFrameLineMatch[0];
  if (topFrame) {
    // Examples of a frame:
    // 1. '\n    at someFunction (chrome-extension://
    //     pafkbggdmjlpgkdkcbjmhmfcdpncadgh/background.js:915:15)\n'
    // 2. '\n    at chrome-extension://pafkbggdmjlpgkdkcbjmhmfcdpncadgh/
    //     utility.js:269:18\n'
    // 3. '\n    at Function.target.(anonymous function) (extensions::
    //     SafeBuiltins:19:14)\n'
    // 4. '\n    at Event.dispatchToListener (event_bindings:382:22)\n'
    var errorLocation;
    // Find the the parentheses at the end of the line, if any.
    var parenthesesMatch = topFrame.match(/\(.*\)\n/);
    if (parenthesesMatch && parenthesesMatch[0]) {
      errorLocation =
          parenthesesMatch[0].substring(1, parenthesesMatch[0].length - 2);
    } else {
      errorLocation = topFrame;
    }

    var topFrameElements = errorLocation.split(':');
    // topFrameElements is an array that ends like:
    // [N-3] //pafkbggdmjlpgkdkcbjmhmfcdpncadgh/utility.js
    // [N-2] 308
    // [N-1] 19
    if (topFrameElements.length >= 3) {
      file = topFrameElements[topFrameElements.length - 3];
      line = topFrameElements[topFrameElements.length - 2];
    }
  }

  var errorText = error.name;
  if (error.canSendMessageToServer)
    errorText = errorText + ': ' + error.message;

  var errorObject = {
    message: errorText,
    file: file,
    line: line,
    trace: filteredStack
  };

  // We use relatively direct calls here because the instrumentation may be in
  // a bad state. Wrappers and promises should not be involved in the reporting.
  var request = buildServerRequest('POST', 'jserrors', 'application/json');
  request.onloadend = function(event) {
    console.log('sendErrorReport status: ' + request.status);
  };

  chrome.identity.getAuthToken({interactive: false}, function(token) {
    if (token) {
      request.setRequestHeader('Authorization', 'Bearer ' + token);
      request.send(JSON.stringify(errorObject));
    }
  });
}

// Limiting 1 error report per background page load.
var errorReported = false;

/**
 * Reports an error to the server and the user, as appropriate.
 * @param {Error} error Error to report.
 */
function reportError(error) {
  var message = 'Critical error:\n' + error.stack;
  if (isInDebugMode())
    console.error(message);

  if (!errorReported) {
    errorReported = true;
    chrome.metricsPrivate.getIsCrashReportingEnabled(function(isEnabled) {
      if (isEnabled)
        sendErrorReport(error);
      if (isInDebugMode())
        alert(message);
    });
  }
}

// Partial mirror of chrome.* for all instrumented functions.
var instrumented = {};

/**
 * Wrapper plugin. These plugins extend instrumentation added by
 * wrapper.wrapCallback by adding code that executes before and after the call
 * to the original callback provided by the extension.
 *
 * @typedef {{
 *   prologue: function (),
 *   epilogue: function ()
 * }}
 */
var WrapperPlugin;

/**
 * Wrapper for callbacks. Used to add error handling and other services to
 * callbacks for HTML and Chrome functions and events.
 */
var wrapper = (function() {
  /**
   * Factory for wrapper plugins. If specified, it's used to generate an
   * instance of WrapperPlugin each time we wrap a callback (which corresponds
   * to addListener call for Chrome events, and to every API call that specifies
   * a callback). WrapperPlugin's lifetime ends when the callback for which it
   * was generated, exits. It's possible to have several instances of
   * WrapperPlugin at the same time.
   * An instance of WrapperPlugin can have state that can be shared by its
   * constructor, prologue() and epilogue(). Also WrapperPlugins can change
   * state of other objects, for example, to do refcounting.
   * @type {?function(): WrapperPlugin}
   */
  var wrapperPluginFactory = null;

  /**
   * Registers a wrapper plugin factory.
   * @param {function(): WrapperPlugin} factory Wrapper plugin factory.
   */
  function registerWrapperPluginFactory(factory) {
    if (wrapperPluginFactory) {
      reportError(buildErrorWithMessageForServer(
          'registerWrapperPluginFactory: factory is already registered.'));
    }

    wrapperPluginFactory = factory;
  }

  /**
   * True if currently executed code runs in a callback or event handler that
   * was instrumented by wrapper.wrapCallback() call.
   * @type {boolean}
   */
  var isInWrappedCallback = false;

  /**
   * Required callbacks that are not yet called. Includes both task and non-task
   * callbacks. This is a map from unique callback id to the stack at the moment
   * when the callback was wrapped. This stack identifies the callback.
   * Used only for diagnostics.
   * @type {Object.<number, string>}
   */
  var pendingCallbacks = {};

  /**
   * Unique ID of the next callback.
   * @type {number}
   */
  var nextCallbackId = 0;

  /**
   * Gets diagnostic string with the status of the wrapper.
   * @return {string} Diagnostic string.
   */
  function debugGetStateString() {
    return 'pendingCallbacks @' + Date.now() + ' = ' +
        JSON.stringify(pendingCallbacks);
  }

  /**
   * Checks that we run in a wrapped callback.
   */
  function checkInWrappedCallback() {
    if (!isInWrappedCallback) {
      reportError(buildErrorWithMessageForServer(
          'Not in instrumented callback'));
    }
  }

  /**
   * Adds error processing to an API callback.
   * @param {Function} callback Callback to instrument.
   * @param {boolean=} opt_isEventListener True if the callback is a listener to
   *     a Chrome API event.
   * @return {Function} Instrumented callback.
   */
  function wrapCallback(callback, opt_isEventListener) {
    var callbackId = nextCallbackId++;

    if (!opt_isEventListener) {
      checkInWrappedCallback();
      pendingCallbacks[callbackId] = new Error().stack + ' @' + Date.now();
    }

    // wrapperPluginFactory may be null before task manager is built, and in
    // tests.
    var wrapperPluginInstance = wrapperPluginFactory && wrapperPluginFactory();

    return function() {
      // This is the wrapper for the callback.
      try {
        verify(!isInWrappedCallback, 'Re-entering instrumented callback');
        isInWrappedCallback = true;

        if (!opt_isEventListener)
          delete pendingCallbacks[callbackId];

        if (wrapperPluginInstance)
          wrapperPluginInstance.prologue();

        // Call the original callback.
        var returnValue = callback.apply(null, arguments);

        if (wrapperPluginInstance)
          wrapperPluginInstance.epilogue();

        verify(isInWrappedCallback,
               'Instrumented callback is not instrumented upon exit');
        isInWrappedCallback = false;

        return returnValue;
      } catch (error) {
        reportError(error);
      }
    };
  }

  /**
   * Returns an instrumented function.
   * @param {!Array.<string>} functionIdentifierParts Path to the chrome.*
   *     function.
   * @param {string} functionName Name of the chrome API function.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   * @return {Function} An instrumented function.
   */
  function createInstrumentedFunction(
      functionIdentifierParts,
      functionName,
      callbackParameter) {
    return function() {
      // This is the wrapper for the API function. Pass the wrapped callback to
      // the original function.
      var callback = arguments[callbackParameter];
      if (typeof callback != 'function') {
        reportError(buildErrorWithMessageForServer(
            'Argument ' + callbackParameter + ' of ' +
            functionIdentifierParts.join('.') + '.' + functionName +
            ' is not a function'));
      }
      arguments[callbackParameter] = wrapCallback(
          callback, functionName == 'addListener');

      var chromeContainer = chrome;
      functionIdentifierParts.forEach(function(fragment) {
        chromeContainer = chromeContainer[fragment];
      });
      return chromeContainer[functionName].
          apply(chromeContainer, arguments);
    };
  }

  /**
   * Instruments an API function to add error processing to its user
   * code-provided callback.
   * @param {string} functionIdentifier Full identifier of the function without
   *     the 'chrome.' portion.
   * @param {number} callbackParameter Index of the callback parameter to this
   *     API function.
   */
  function instrumentChromeApiFunction(functionIdentifier, callbackParameter) {
    var functionIdentifierParts = functionIdentifier.split('.');
    var functionName = functionIdentifierParts.pop();
    var chromeContainer = chrome;
    var instrumentedContainer = instrumented;
    functionIdentifierParts.forEach(function(fragment) {
      chromeContainer = chromeContainer[fragment];
      if (!chromeContainer) {
        reportError(buildErrorWithMessageForServer(
            'Cannot instrument ' + functionIdentifier));
      }

      if (!(fragment in instrumentedContainer))
        instrumentedContainer[fragment] = {};

      instrumentedContainer = instrumentedContainer[fragment];
    });

    var targetFunction = chromeContainer[functionName];
    if (!targetFunction) {
      reportError(buildErrorWithMessageForServer(
          'Cannot instrument ' + functionIdentifier));
    }

    instrumentedContainer[functionName] = createInstrumentedFunction(
        functionIdentifierParts,
        functionName,
        callbackParameter);
  }

  instrumentChromeApiFunction('runtime.onSuspend.addListener', 0);

  instrumented.runtime.onSuspend.addListener(function() {
    var stringifiedPendingCallbacks = JSON.stringify(pendingCallbacks);
    verify(
        stringifiedPendingCallbacks == '{}',
        'Pending callbacks when unloading event page @' + Date.now() + ':' +
        stringifiedPendingCallbacks);
  });

  return {
    wrapCallback: wrapCallback,
    instrumentChromeApiFunction: instrumentChromeApiFunction,
    registerWrapperPluginFactory: registerWrapperPluginFactory,
    checkInWrappedCallback: checkInWrappedCallback,
    debugGetStateString: debugGetStateString
  };
})();

wrapper.instrumentChromeApiFunction('alarms.get', 1);
wrapper.instrumentChromeApiFunction('alarms.onAlarm.addListener', 0);
wrapper.instrumentChromeApiFunction('identity.getAuthToken', 1);
wrapper.instrumentChromeApiFunction('identity.onSignInChanged.addListener', 0);
wrapper.instrumentChromeApiFunction('identity.removeCachedAuthToken', 1);
wrapper.instrumentChromeApiFunction('storage.local.get', 1);
wrapper.instrumentChromeApiFunction('webstorePrivate.getBrowserLogin', 0);

/**
 * Promise adapter for all JS promises to the task manager.
 */
function registerPromiseAdapter() {
  var originalThen = Promise.prototype.then;
  var originalCatch = Promise.prototype.catch;

  /**
   * Takes a promise and adds the callback tracker to it.
   * @param {object} promise Promise that receives the callback tracker.
   */
  function instrumentPromise(promise) {
    if (promise.__tracker === undefined) {
      promise.__tracker = createPromiseCallbackTracker(promise);
    }
  }

  Promise.prototype.then = function(onResolved, onRejected) {
    instrumentPromise(this);
    return this.__tracker.handleThen(onResolved, onRejected);
  }

  Promise.prototype.catch = function(onRejected) {
    instrumentPromise(this);
    return this.__tracker.handleCatch(onRejected);
  }

  /**
   * Promise Callback Tracker.
   * Handles coordination of 'then' and 'catch' callbacks in a task
   * manager compatible way. For an individual promise, either the 'then'
   * arguments or the 'catch' arguments will be processed, never both.
   *
   * Example:
   *     var p = new Promise([Function]);
   *     p.then([ThenA]);
   *     p.then([ThenB]);
   *     p.catch([CatchA]);
   *     On resolution, [ThenA] and [ThenB] will be used. [CatchA] is discarded.
   *     On rejection, vice versa.
   *
   * Clarification:
   *     Chained promises create a new promise that is tracked separately from
   *     the originaing promise, as the example below demonstrates:
   *
   *     var p = new Promise([Function]));
   *     p.then([ThenA]).then([ThenB]).catch([CatchA]);
   *         ^             ^             ^
   *         |             |             + Returns a new promise.
   *         |             + Returns a new promise.
   *         + Returns a new promise.
   *
   *     Four promises exist in the above statement, each with its own
   *     resolution and rejection state. However, by default, this state is
   *     chained to the previous promise's resolution or rejection
   *     state.
   *
   *     If p resolves, then the 'then' calls will execute until all the 'then'
   *     clauses are executed. If the result of either [ThenA] or [ThenB] is a
   *     promise, then that execution state will guide the remaining chain.
   *     Similarly, if [CatchA] returns a promise, it can also guide the
   *     remaining chain. In this specific case, the chain ends, so there
   *     is nothing left to do.
   * @param {object} promise Promise being tracked.
   * @return {object} A promise callback tracker.
   */
  function createPromiseCallbackTracker(promise) {
    /**
     * Callback Tracker. Holds an array of callbacks created for this promise.
     * The indirection allows quick checks against the array and clearing the
     * array without ugly splicing and copying.
     * @typedef {{
     *   callback: array.<Function>=
     * }}
     */
    var CallbackTracker;

    /** @type {CallbackTracker} */
    var thenTracker = {callbacks: []};
    /** @type {CallbackTracker} */
    var catchTracker = {callbacks: []};

    /**
     * Returns true if the specified value is callable.
     * @param {*} value Value to check.
     * @return {boolean} True if the value is a callable.
     */
    function isCallable(value) {
      return typeof value === 'function';
    }

    /**
     * Takes a tracker and clears its callbacks in a manner consistent with
     * the task manager. For the task manager, it also calls all callbacks
     * by no-oping them first and then calling them.
     * @param {CallbackTracker} tracker Tracker to clear.
     */
    function clearTracker(tracker) {
      if (tracker.callbacks) {
        var callbacksToClear = tracker.callbacks;
        // No-ops all callbacks of this type.
        tracker.callbacks = undefined;
        // Do not wrap the promise then argument!
        // It will call wrapped callbacks.
        originalThen.call(Promise.resolve(), function() {
          for (var i = 0; i < callbacksToClear.length; i++) {
            callbacksToClear[i]();
          }
        });
      }
    }

    /**
     * Takes the argument to a 'then' or 'catch' function and applies
     * a wrapping to callables consistent to ECMA promises.
     * @param {*} maybeCallback Argument to 'then' or 'catch'.
     * @param {CallbackTracker} sameTracker Tracker for the call type.
     *     Example: If the argument is from a 'then' call, use thenTracker.
     * @param {CallbackTracker} otherTracker Tracker for the opposing call type.
     *     Example: If the argument is from a 'then' call, use catchTracker.
     * @return {*} Consumable argument with necessary wrapping applied.
     */
    function registerAndWrapMaybeCallback(
          maybeCallback, sameTracker, otherTracker) {
      // If sameTracker.callbacks is undefined, we've reached an ending state
      // that means this callback will never be called back.
      // We will still forward this call on to let the promise system
      // handle further processing, but since this promise is in an ending state
      // we can be confident it will never be called back.
      if (isCallable(maybeCallback) &&
          !maybeCallback.wrappedByPromiseTracker &&
          sameTracker.callbacks) {
        var handler = wrapper.wrapCallback(function() {
          if (sameTracker.callbacks) {
            clearTracker(otherTracker);
            return maybeCallback.apply(null, arguments);
          }
        }, false);
        // Harmony promises' catch calls will call into handleThen,
        // double-wrapping all catch callbacks. Regular promise catch calls do
        // not call into handleThen. Setting an attribute on the wrapped
        // function is compatible with both promise implementations.
        handler.wrappedByPromiseTracker = true;
        sameTracker.callbacks.push(handler);
        return handler;
      } else {
        return maybeCallback;
      }
    }

    /**
     * Tracks then calls equivalent to Promise.prototype.then.
     * @param {*} onResolved Argument to use if the promise is resolved.
     * @param {*} onRejected Argument to use if the promise is rejected.
     * @return {object} Promise resulting from the 'then' call.
     */
    function handleThen(onResolved, onRejected) {
      var resolutionHandler =
          registerAndWrapMaybeCallback(onResolved, thenTracker, catchTracker);
      var rejectionHandler =
          registerAndWrapMaybeCallback(onRejected, catchTracker, thenTracker);
      return originalThen.call(promise, resolutionHandler, rejectionHandler);
    }

    /**
     * Tracks then calls equivalent to Promise.prototype.catch.
     * @param {*} onRejected Argument to use if the promise is rejected.
     * @return {object} Promise resulting from the 'catch' call.
     */
    function handleCatch(onRejected) {
      var rejectionHandler =
          registerAndWrapMaybeCallback(onRejected, catchTracker, thenTracker);
      return originalCatch.call(promise, rejectionHandler);
    }

    // Register at least one resolve and reject callback so we always receive
    // a callback to update the task manager and clear the callbacks
    // that will never occur.
    //
    // The then form is used to avoid reentrancy by handleCatch,
    // which ends up calling handleThen.
    handleThen(function() {}, function() {});

    return {
      handleThen: handleThen,
      handleCatch: handleCatch
    };
  }
}

registerPromiseAdapter();

/**
 * Control promise rejection.
 * @enum {number}
 */
var PromiseRejection = {
  /** Disallow promise rejection */
  DISALLOW: 0,
  /** Allow promise rejection */
  ALLOW: 1
};

/**
 * Provides the promise equivalent of instrumented.storage.local.get.
 * @param {Object} defaultStorageObject Default storage object to fill.
 * @param {PromiseRejection=} opt_allowPromiseRejection If
 *     PromiseRejection.ALLOW, allow promise rejection on errors, otherwise the
 *     default storage object is resolved.
 * @return {Promise} A promise that fills the default storage object. On
 *     failure, if promise rejection is allowed, the promise is rejected,
 *     otherwise it is resolved to the default storage object.
 */
function fillFromChromeLocalStorage(
    defaultStorageObject,
    opt_allowPromiseRejection) {
  return new Promise(function(resolve, reject) {
    // We have to create a keys array because keys with a default value
    // of undefined will cause that key to not be looked up!
    var keysToGet = [];
    for (var key in defaultStorageObject) {
      keysToGet.push(key);
    }
    instrumented.storage.local.get(keysToGet, function(items) {
      if (items) {
        // Merge the result with the default storage object to ensure all keys
        // requested have either the default value or the retrieved storage
        // value.
        var result = {};
        for (var key in defaultStorageObject) {
          result[key] = (key in items) ? items[key] : defaultStorageObject[key];
        }
        resolve(result);
      } else if (opt_allowPromiseRejection === PromiseRejection.ALLOW) {
        reject();
      } else {
        resolve(defaultStorageObject);
      }
    });
  });
}

/**
 * Builds the object to manage tasks (mutually exclusive chains of events).
 * @param {function(string, string): boolean} areConflicting Function that
 *     checks if a new task can't be added to a task queue that contains an
 *     existing task.
 * @return {Object} Task manager interface.
 */
function buildTaskManager(areConflicting) {
  /**
   * Queue of scheduled tasks. The first element, if present, corresponds to the
   * currently running task.
   * @type {Array.<Object.<string, function()>>}
   */
  var queue = [];

  /**
   * Count of unfinished callbacks of the current task.
   * @type {number}
   */
  var taskPendingCallbackCount = 0;

  /**
   * True if currently executed code is a part of a task.
   * @type {boolean}
   */
  var isInTask = false;

  /**
   * Starts the first queued task.
   */
  function startFirst() {
    verify(queue.length >= 1, 'startFirst: queue is empty');
    verify(!isInTask, 'startFirst: already in task');
    isInTask = true;

    // Start the oldest queued task, but don't remove it from the queue.
    verify(
        taskPendingCallbackCount == 0,
        'tasks.startFirst: still have pending task callbacks: ' +
        taskPendingCallbackCount +
        ', queue = ' + JSON.stringify(queue) + ', ' +
        wrapper.debugGetStateString());
    var entry = queue[0];
    console.log('Starting task ' + entry.name);

    entry.task();

    verify(isInTask, 'startFirst: not in task at exit');
    isInTask = false;
    if (taskPendingCallbackCount == 0)
      finish();
  }

  /**
   * Checks if a new task can be added to the task queue.
   * @param {string} taskName Name of the new task.
   * @return {boolean} Whether the new task can be added.
   */
  function canQueue(taskName) {
    for (var i = 0; i < queue.length; ++i) {
      if (areConflicting(taskName, queue[i].name)) {
        console.log('Conflict: new=' + taskName +
                    ', scheduled=' + queue[i].name);
        return false;
      }
    }

    return true;
  }

  /**
   * Adds a new task. If another task is not running, runs the task immediately.
   * If any task in the queue is not compatible with the task, ignores the new
   * task. Otherwise, stores the task for future execution.
   * @param {string} taskName Name of the task.
   * @param {function()} task Function to run.
   */
  function add(taskName, task) {
    wrapper.checkInWrappedCallback();
    console.log('Adding task ' + taskName);
    if (!canQueue(taskName))
      return;

    queue.push({name: taskName, task: task});

    if (queue.length == 1) {
      startFirst();
    }
  }

  /**
   * Completes the current task and starts the next queued task if available.
   */
  function finish() {
    verify(queue.length >= 1,
           'tasks.finish: The task queue is empty');
    console.log('Finishing task ' + queue[0].name);
    queue.shift();

    if (queue.length >= 1)
      startFirst();
  }

  instrumented.runtime.onSuspend.addListener(function() {
    verify(
        queue.length == 0,
        'Incomplete task when unloading event page,' +
        ' queue = ' + JSON.stringify(queue) + ', ' +
        wrapper.debugGetStateString());
  });


  /**
   * Wrapper plugin for tasks.
   * @constructor
   */
  function TasksWrapperPlugin() {
    this.isTaskCallback = isInTask;
    if (this.isTaskCallback)
      ++taskPendingCallbackCount;
  }

  TasksWrapperPlugin.prototype = {
    /**
     * Plugin code to be executed before invoking the original callback.
     */
    prologue: function() {
      if (this.isTaskCallback) {
        verify(!isInTask, 'TasksWrapperPlugin.prologue: already in task');
        isInTask = true;
      }
    },

    /**
     * Plugin code to be executed after invoking the original callback.
     */
    epilogue: function() {
      if (this.isTaskCallback) {
        verify(isInTask, 'TasksWrapperPlugin.epilogue: not in task at exit');
        isInTask = false;
        if (--taskPendingCallbackCount == 0)
          finish();
      }
    }
  };

  wrapper.registerWrapperPluginFactory(function() {
    return new TasksWrapperPlugin();
  });

  return {
    add: add
  };
}

/**
 * Builds an object to manage retrying activities with exponential backoff.
 * @param {string} name Name of this attempt manager.
 * @param {function()} attempt Activity that the manager retries until it
 *     calls 'stop' method.
 * @param {number} initialDelaySeconds Default first delay until first retry.
 * @param {number} maximumDelaySeconds Maximum delay between retries.
 * @return {Object} Attempt manager interface.
 */
function buildAttemptManager(
    name, attempt, initialDelaySeconds, maximumDelaySeconds) {
  var alarmName = 'attempt-scheduler-' + name;
  var currentDelayStorageKey = 'current-delay-' + name;

  /**
   * Creates an alarm for the next attempt. The alarm is repeating for the case
   * when the next attempt crashes before registering next alarm.
   * @param {number} delaySeconds Delay until next retry.
   */
  function createAlarm(delaySeconds) {
    var alarmInfo = {
      delayInMinutes: delaySeconds / 60,
      periodInMinutes: maximumDelaySeconds / 60
    };
    chrome.alarms.create(alarmName, alarmInfo);
  }

  /**
   * Indicates if this attempt manager has started.
   * @param {function(boolean)} callback The function's boolean parameter is
   *     true if the attempt manager has started, false otherwise.
   */
  function isRunning(callback) {
    instrumented.alarms.get(alarmName, function(alarmInfo) {
      callback(!!alarmInfo);
    });
  }

  /**
   * Schedules the alarm with a random factor to reduce the chance that all
   * clients will fire their timers at the same time.
   * @param {number} durationSeconds Number of seconds before firing the alarm.
   */
  function scheduleAlarm(durationSeconds) {
    var randomizedRetryDuration =
        Math.min(durationSeconds * (1 + 0.2 * Math.random()),
                 maximumDelaySeconds);

    createAlarm(randomizedRetryDuration);

    var items = {};
    items[currentDelayStorageKey] = randomizedRetryDuration;
    chrome.storage.local.set(items);
  }

  /**
   * Starts repeated attempts.
   * @param {number=} opt_firstDelaySeconds Time until the first attempt, if
   *     specified. Otherwise, initialDelaySeconds will be used for the first
   *     attempt.
   */
  function start(opt_firstDelaySeconds) {
    if (opt_firstDelaySeconds) {
      createAlarm(opt_firstDelaySeconds);
      chrome.storage.local.remove(currentDelayStorageKey);
    } else {
      scheduleAlarm(initialDelaySeconds);
    }
  }

  /**
   * Stops repeated attempts.
   */
  function stop() {
    chrome.alarms.clear(alarmName);
    chrome.storage.local.remove(currentDelayStorageKey);
  }

  /**
   * Schedules an exponential backoff retry.
   * @return {Promise} A promise to schedule the retry.
   */
  function scheduleRetry() {
    var request = {};
    request[currentDelayStorageKey] = undefined;
    return fillFromChromeLocalStorage(request, PromiseRejection.ALLOW)
        .catch(function() {
          request[currentDelayStorageKey] = maximumDelaySeconds;
          return Promise.resolve(request);
        })
        .then(function(items) {
          console.log('scheduleRetry-get-storage ' + JSON.stringify(items));
          var retrySeconds = initialDelaySeconds;
          if (items[currentDelayStorageKey]) {
            retrySeconds = items[currentDelayStorageKey] * 2;
          }
          scheduleAlarm(retrySeconds);
        });
  }

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == alarmName)
      isRunning(function(running) {
        if (running)
          attempt();
      });
  });

  return {
    start: start,
    scheduleRetry: scheduleRetry,
    stop: stop,
    isRunning: isRunning
  };
}

// TODO(robliao): Use signed-in state change watch API when it's available.
/**
 * Wraps chrome.identity to provide limited listening support for
 * the sign in state by polling periodically for the auth token.
 * @return {Object} The Authentication Manager interface.
 */
function buildAuthenticationManager() {
  var alarmName = 'sign-in-alarm';

  /**
   * Gets an OAuth2 access token.
   * @return {Promise} A promise to get the authentication token. If there is
   *     no token, the request is rejected.
   */
  function getAuthToken() {
    return new Promise(function(resolve, reject) {
      instrumented.identity.getAuthToken({interactive: false}, function(token) {
        if (chrome.runtime.lastError || !token) {
          reject();
        } else {
          resolve(token);
        }
      });
    });
  }

  /**
   * Determines whether there is an account attached to the profile.
   * @return {Promise} A promise to determine if there is an account attached
   *     to the profile.
   */
  function isSignedIn() {
    return new Promise(function(resolve) {
      instrumented.webstorePrivate.getBrowserLogin(function(accountInfo) {
        resolve(!!accountInfo.login);
      });
    });
  }

  /**
   * Removes the specified cached token.
   * @param {string} token Authentication Token to remove from the cache.
   * @return {Promise} A promise that resolves on completion.
   */
  function removeToken(token) {
    return new Promise(function(resolve) {
      instrumented.identity.removeCachedAuthToken({token: token}, function() {
        // Let Chrome know about a possible problem with the token.
        getAuthToken();
        resolve();
      });
    });
  }

  var listeners = [];

  /**
   * Registers a listener that gets called back when the signed in state
   * is found to be changed.
   * @param {function()} callback Called when the answer to isSignedIn changes.
   */
  function addListener(callback) {
    listeners.push(callback);
  }

  /**
   * Checks if the last signed in state matches the current one.
   * If it doesn't, it notifies the listeners of the change.
   */
  function checkAndNotifyListeners() {
    isSignedIn().then(function(signedIn) {
      fillFromChromeLocalStorage({lastSignedInState: undefined})
          .then(function(items) {
            if (items.lastSignedInState != signedIn) {
              chrome.storage.local.set(
                  {lastSignedInState: signedIn});
              listeners.forEach(function(callback) {
                callback();
              });
            }
        });
      });
  }

  instrumented.identity.onSignInChanged.addListener(function() {
    checkAndNotifyListeners();
  });

  instrumented.alarms.onAlarm.addListener(function(alarm) {
    if (alarm.name == alarmName)
      checkAndNotifyListeners();
  });

  // Poll for the sign in state every hour.
  // One hour is just an arbitrary amount of time chosen.
  chrome.alarms.create(alarmName, {periodInMinutes: 60});

  return {
    addListener: addListener,
    getAuthToken: getAuthToken,
    isSignedIn: isSignedIn,
    removeToken: removeToken
  };
}
