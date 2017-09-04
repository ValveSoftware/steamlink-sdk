// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *  @fileoverview LIS Standalone hack
 *  This file contains the code necessary to make the Touch LIS work
 *  as a stand-alone application (as opposed to being embedded into chrome).
 *  This is useful for rapid development and testing, but does not actually form
 *  part of the product.
 */

// Note that this file never gets concatenated and embeded into Chrome, so we
// can enable strict mode for the whole file just like normal.
'use strict';

/**
 * For non-Chrome browsers, create a dummy chrome object
 */
if (!window.chrome) {
  var chrome = {};
}

/**
 *  A replacement chrome.send method that supplies static data for the
 *  key APIs used by the LIS.
 *
 *  Note that the real chrome object also supplies data for most-viewed and
 *  recently-closed pages, but the tangent LIS doesn't use that data so we
 *  don't bother simulating it here.
 *
 *  We create this object by applying an anonymous function so that we can have
 *  local variables (avoid polluting the global object)
 */
chrome.send = (function() {
  var users = [
  {
    name: 'Alan Beaker',
    emailAddress: 'beaker@chromium.org',
    imageUrl: '../../app/theme/avatar_beaker.png',
    canRemove: false
  },
  {
    name: 'Alex Briefcase',
    emailAddress: 'briefcase@chromium.org',
    imageUrl: '../../app/theme/avatar_briefcase.png',
    canRemove: true
  },
  {
    name: 'Alex Circles',
    emailAddress: 'circles@chromium.org',
    imageUrl: '../../app/theme/avatar_circles.png',
    canRemove: true
  },
  {
    name: 'Guest',
    emailAddress: '',
    imageUrl: '',
    canRemove: false
  }
  ];

  /**
   * Invoke the getAppsCallback function with a snapshot of the current app
   * database.
   */
  function sendGetUsersCallback()
  {
    // We don't want to hand out our array directly because the NTP will
    // assume it owns the array and is free to modify it.  For now we make a
    // one-level deep copy of the array (since cloning the whole thing is
    // more work and unnecessary at the moment).
    getUsersCallback(users.slice(0));
  }

  /**
   * Like Array.prototype.indexOf but calls a predicate to test for match
   *
   * @param {Array} array The array to search.
   * @param {function(Object): boolean} predicate The function to invoke on
   *     each element.
   * @return {number} First index at which predicate returned true, or -1.
   */
  function indexOfPred(array, predicate) {
    for (var i = 0; i < array.length; i++) {
      if (predicate(array[i]))
        return i;
    }
    return -1;
  }

  /**
   * Get index into apps of an application object
   * Requires the specified app to be present
   *
   * @param {string} id The ID of the application to locate.
   * @return {number} The index in apps for an object with the specified ID.
   */
  function getUserIndex(name) {
    var i = indexOfPred(apps, function(e) { return e.name === name;});
    if (i == -1)
      alert('Error: got unexpected App ID');
    return i;
  }

  /**
   * Get an user object given the user name
   * Requires
   * @param {string} name The user name to search for.
   * @return {Object} The corresponding user object.
   */
  function getUser(name) {
    return users[getUserIndex(name)];
  }

  /**
   * Simlulate the login of a user
   *
   * @param {string} email_address the email address of the user logging in.
   * @param {string} password the password of the user logging in.
   */
  function login(email_address, password) {
    console.log('password', password);
    if (password == 'correct') {
      return true;
    }
    return false;
  }

  /**
   * The chrome server communication entrypoint.
   *
   * @param {string} command Name of the command to send.
   * @param {Array} args Array of command-specific arguments.
   */
  return function(command, args) {
    // Chrome API is async
    window.setTimeout(function() {
      switch (command) {
        // called to populate the list of applications
        case 'GetUsers':
          sendGetUsersCallback();
          break;

        // Called when a user is removed.
        case 'RemoveUser':
          break;

        // Called when a user attempts to login.
        case 'Login':
          login(args[0], args[1]);
          break;

        // Called when an app is moved to a different page
        case 'MoveUser':
          break;

        case 'SetGuestPosition':
          break;

        default:
          throw new Error('Unexpected chrome command: ' + command);
          break;
      }
    }, 0);
  };
})();

/*
 * On iOS we need a hack to avoid spurious click events
 * In particular, if the user delays briefly between first touching and starting
 * to drag, when the user releases a click event will be generated.
 * Note that this seems to happen regardless of whether we do preventDefault on
 * touchmove events.
 */
if (/iPhone|iPod|iPad/.test(navigator.userAgent) &&
    !(/Chrome/.test(navigator.userAgent))) {
  // We have a real iOS device (no a ChromeOS device pretending to be iOS)
  (function() {
    // True if a gesture is occuring that should cause clicks to be swallowed
    var gestureActive = false;

    // The position a touch was last started
    var lastTouchStartPosition;

    // Distance which a touch needs to move to be considered a drag
    var DRAG_DISTANCE = 3;

    document.addEventListener('touchstart', function(event) {
      lastTouchStartPosition = {
        x: event.touches[0].clientX,
        y: event.touches[0].clientY
      };
      // A touchstart ALWAYS preceeds a click (valid or not), so cancel any
      // outstanding gesture. Also, any multi-touch is a gesture that should
      // prevent clicks.
      gestureActive = event.touches.length > 1;
    }, true);

    document.addEventListener('touchmove', function(event) {
      // When we see a move, measure the distance from the last touchStart
      // If this is a multi-touch then the work here is irrelevant
      // (gestureActive is already true)
      var t = event.touches[0];
      if (Math.abs(t.clientX - lastTouchStartPosition.x) > DRAG_DISTANCE ||
          Math.abs(t.clientY - lastTouchStartPosition.y) > DRAG_DISTANCE) {
        gestureActive = true;
      }
    }, true);

    document.addEventListener('click', function(event) {
      // If we got here without gestureActive being set then it means we had
      // a touchStart without any real dragging before touchEnd - we can allow
      // the click to proceed.
      if (gestureActive) {
        event.preventDefault();
        event.stopPropagation();
      }
    }, true);
  })();
}

/*  Hack to add Element.classList to older browsers that don't yet support it.
    From https://developer.mozilla.org/en/DOM/element.classList.
*/
if (typeof Element !== 'undefined' &&
    !Element.prototype.hasOwnProperty('classList')) {
  (function() {
    var classListProp = 'classList',
        protoProp = 'prototype',
        elemCtrProto = Element[protoProp],
        objCtr = Object,
        strTrim = String[protoProp].trim || function() {
          return this.replace(/^\s+|\s+$/g, '');
        },
        arrIndexOf = Array[protoProp].indexOf || function(item) {
          for (var i = 0, len = this.length; i < len; i++) {
            if (i in this && this[i] === item) {
              return i;
            }
          }
          return -1;
        },
        // Vendors: please allow content code to instantiate DOMExceptions
        /** @constructor  */
        DOMEx = function(type, message) {
          this.name = type;
          this.code = DOMException[type];
          this.message = message;
        },
        checkTokenAndGetIndex = function(classList, token) {
          if (token === '') {
            throw new DOMEx(
                'SYNTAX_ERR',
                'An invalid or illegal string was specified'
            );
          }
          if (/\s/.test(token)) {
            throw new DOMEx(
                'INVALID_CHARACTER_ERR',
                'String contains an invalid character'
            );
          }
          return arrIndexOf.call(classList, token);
        },
        /** @constructor
         *  @extends {Array} */
        ClassList = function(elem) {
          var trimmedClasses = strTrim.call(elem.className),
              classes = trimmedClasses ? trimmedClasses.split(/\s+/) : [];

          for (var i = 0, len = classes.length; i < len; i++) {
            this.push(classes[i]);
          }
          this._updateClassName = function() {
            elem.className = this.toString();
          };
        },
        classListProto = ClassList[protoProp] = [],
        classListGetter = function() {
          return new ClassList(this);
        };

    // Most DOMException implementations don't allow calling DOMException's
    // toString() on non-DOMExceptions. Error's toString() is sufficient here.
    DOMEx[protoProp] = Error[protoProp];
    classListProto.item = function(i) {
      return this[i] || null;
    };
    classListProto.contains = function(token) {
      token += '';
      return checkTokenAndGetIndex(this, token) !== -1;
    };
    classListProto.add = function(token) {
      token += '';
      if (checkTokenAndGetIndex(this, token) === -1) {
        this.push(token);
        this._updateClassName();
      }
    };
    classListProto.remove = function(token) {
      token += '';
      var index = checkTokenAndGetIndex(this, token);
      if (index !== -1) {
        this.splice(index, 1);
        this._updateClassName();
      }
    };
    classListProto.toggle = function(token) {
      token += '';
      if (checkTokenAndGetIndex(this, token) === -1) {
        this.add(token);
      } else {
        this.remove(token);
      }
    };
    classListProto.toString = function() {
      return this.join(' ');
    };

    if (objCtr.defineProperty) {
      var classListDescriptor = {
        get: classListGetter,
        enumerable: true,
        configurable: true
      };
      objCtr.defineProperty(elemCtrProto, classListProp, classListDescriptor);
    } else if (objCtr[protoProp].__defineGetter__) {
      elemCtrProto.__defineGetter__(classListProp, classListGetter);
    }
  }());
}

/* Hack to add Function.bind to older browsers that don't yet support it. From:
   https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Function/bind
*/
if (!Function.prototype.bind) {
  /**
   * @param {Object} selfObj Specifies the object which |this| should
   *     point to when the function is run. If the value is null or undefined,
   *     it will default to the global object.
   * @param {...*} var_args Additional arguments that are partially
   *     applied to the function.
   * @return {!Function} A partially-applied form of the function bind() was
   *     invoked as a method of.
   *  @suppress {duplicate}
   */
  Function.prototype.bind = function(selfObj, var_args) {
    var slice = [].slice,
        args = slice.call(arguments, 1),
        self = this,
        /** @constructor  */
        nop = function() {},
        bound = function() {
          return self.apply(this instanceof nop ? this : (selfObj || {}),
                              args.concat(slice.call(arguments)));
        };
    nop.prototype = self.prototype;
    bound.prototype = new nop();
    return bound;
  };
}

