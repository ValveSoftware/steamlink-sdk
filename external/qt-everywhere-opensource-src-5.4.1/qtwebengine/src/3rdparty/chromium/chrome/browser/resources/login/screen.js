// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for all login WebUI screens.
 */
cr.define('login', function() {
  var Screen = cr.ui.define('div');

  /** @const */ var CALLBACK_USER_ACTED = 'userActed';
  /** @const */ var CALLBACK_CONTEXT_CHANGED = 'contextChanged';

  function doNothing() {};

  var querySelectorAll = HTMLDivElement.prototype.querySelectorAll;

  Screen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Prefix added to sent to Chrome messages' names.
     */
    sendPrefix_: '',

    /**
     * Context used by this screen.
     */
    context_: null,

    /**
     * Dictionary of context observers that are methods of |this| bound to
     * |this|.
     */
    contextObservers_: {},

    get context() {
      return this.context_;
    },

    /**
     * Sends recent context changes to C++ handler.
     */
    commitContextChanges: function() {
      if (!this.context_.hasChanges())
        return;
      this.send(CALLBACK_CONTEXT_CHANGED, this.context_.getChangesAndReset());
    },

    /**
     * Sends message to Chrome, adding needed prefix to message name. All
     * arguments after |messageName| are packed into message parameters list.
     *
     * @param {string} messageName Name of message without a prefix.
     * @param {...*} varArgs parameters for message.
     */
    send: function(messageName, varArgs) {
      if (arguments.length == 0)
        throw Error('Message name is not provided.');
      var fullMessageName = this.sendPrefix_ + messageName;
      var payload = Array.prototype.slice.call(arguments, 1);
      chrome.send(fullMessageName, payload);
    },

    decorate: doNothing,

    /**
     * Returns minimal size that screen prefers to have. Default implementation
     * returns current screen size.
     * @return {{width: number, height: number}}
     */
    getPreferredSize: function() {
      return {width: this.offsetWidth, height: this.offsetHeight};
    },

    /**
     * Called for currently active screen when screen size changed.
     */
    onWindowResize: doNothing,

    /**
     * Does the following things:
     *  * Creates screen context.
     *  * Looks for elements having "alias" property and adds them as the
     *    proprties of the screen with name equal to value of "alias", i.e. HTML
     *    element <div alias="myDiv"></div> will be stored in this.myDiv.
     *  * Looks for buttons having "action" properties and adds click handlers
     *    to them. These handlers send |CALLBACK_USER_ACTED| messages to
     *    C++ with "action" property's value as payload.
     */
    initialize: function() {
      this.context_ = new login.ScreenContext();
      this.querySelectorAll('[alias]').forEach(function(element) {
        this[element.getAttribute('alias')] = element;
      }, this);
      var self = this;
      this.querySelectorAll('button[action]').forEach(function(button) {
        button.addEventListener('click', function(e) {
          var action = this.getAttribute('action');
          self.send(CALLBACK_USER_ACTED, action);
          e.stopPropagation();
        });
      });
    },

    /**
     * Starts observation of property with |key| of the context attached to
     * current screen. This method differs from "login.ScreenContext" in that
     * it automatically detects if observer is method of |this| and make
     * all needed actions to make it work correctly. So it's no need for client
     * to bind methods to |this| and keep resulting callback for
     * |removeObserver| call:
     *
     *   this.addContextObserver('key', this.onKeyChanged_);
     *   ...
     *   this.removeContextObserver('key', this.onKeyChanged_);
     */
    addContextObserver: function(key, observer) {
      var realObserver = observer;
      var propertyName = this.getPropertyNameOf_(observer);
      if (propertyName) {
        if (!this.contextObservers_.hasOwnProperty(propertyName))
          this.contextObservers_[propertyName] = observer.bind(this);
        realObserver = this.contextObservers_[propertyName];
      }
      this.context.addObserver(key, realObserver);
    },

    /**
     * Removes |observer| from the list of context observers. Supports not only
     * regular functions but also screen methods (see comment to
     * |addContextObserver|).
     */
    removeContextObserver: function(observer) {
      var realObserver = observer;
      var propertyName = this.getPropertyNameOf_(observer);
      if (propertyName) {
        if (!this.contextObservers_.hasOwnProperty(propertyName))
          return;
        realObserver = this.contextObservers_[propertyName];
        delete this.contextObservers_[propertyName];
      }
      this.context.removeObserver(realObserver);
    },

    /**
     * Calls standart |querySelectorAll| method and returns its result converted
     * to Array.
     */
    querySelectorAll: function(selector) {
      var list = querySelectorAll.call(this, selector);
      return Array.prototype.slice.call(list);
    },

    /**
     * Called when context changes are recieved from C++.
     * @private
     */
    contextChanged_: function(diff) {
      this.context.applyChanges(diff);
    },

    /**
     * If |value| is the value of some property of |this| returns property's
     * name. Otherwise returns empty string.
     * @private
     */
    getPropertyNameOf_: function(value) {
      for (var key in this)
        if (this[key] === value)
          return key;
      return '';
    }
  };

  Screen.CALLBACK_USER_ACTED = CALLBACK_USER_ACTED;

  return {
    Screen: Screen
  };
});

cr.define('login', function() {
  return {
    /**
     * Creates class and object for screen.
     * Methods specified in EXTERNAL_API array of prototype
     * will be available from C++ part.
     * Example:
     *     login.createScreen('ScreenName', 'screen-id', {
     *       foo: function() { console.log('foo'); },
     *       bar: function() { console.log('bar'); }
     *       EXTERNAL_API: ['foo'];
     *     });
     *     login.ScreenName.register();
     *     var screen = $('screen-id');
     *     screen.foo(); // valid
     *     login.ScreenName.foo(); // valid
     *     screen.bar(); // valid
     *     login.ScreenName.bar(); // invalid
     *
     * @param {string} name Name of created class.
     * @param {string} id Id of div representing screen.
     * @param {(function()|Object)} proto Prototype of object or function that
     *     returns prototype.
     */
    createScreen: function(name, id, proto) {
      if (typeof proto == 'function')
        proto = proto();
      cr.define('login', function() {
        var api = proto.EXTERNAL_API || [];
        for (var i = 0; i < api.length; ++i) {
          var methodName = api[i];
          if (typeof proto[methodName] !== 'function')
            throw Error('External method "' + methodName + '" for screen "' +
                name + '" not a function or undefined.');
        }

        var constructor = cr.ui.define(login.Screen);
        constructor.prototype = Object.create(login.Screen.prototype);
        Object.getOwnPropertyNames(proto).forEach(function(propertyName) {
          var descriptor =
              Object.getOwnPropertyDescriptor(proto, propertyName);
          Object.defineProperty(constructor.prototype,
              propertyName, descriptor);
          if (api.indexOf(propertyName) >= 0) {
            constructor[propertyName] = (function(x) {
              return function() {
                var screen = $(id);
                return screen[x].apply(screen, arguments);
              };
            })(propertyName);
          }
        });
        constructor.contextChanged = function() {
          var screen = $(id);
          screen.contextChanged_.apply(screen, arguments);
        }
        constructor.prototype.name = function() { return id; };
        constructor.prototype.sendPrefix_ = 'login.' + name + '.';

        constructor.register = function() {
          var screen = $(id);
          constructor.decorate(screen);
          Oobe.getInstance().registerScreen(screen);
        };

        var result = {};
        result[name] = constructor;
        return result;
      });
    }
  };
});

