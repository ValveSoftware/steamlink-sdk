// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nativeDeepCopy = requireNative('utils').deepCopy;
var schemaRegistry = requireNative('schema_registry');
var CHECK = requireNative('logging').CHECK;
var DCHECK = requireNative('logging').DCHECK;
var WARNING = requireNative('logging').WARNING;

/**
 * An object forEach. Calls |f| with each (key, value) pair of |obj|, using
 * |self| as the target.
 * @param {Object} obj The object to iterate over.
 * @param {function} f The function to call in each iteration.
 * @param {Object} self The object to use as |this| in each function call.
 */
function forEach(obj, f, self) {
  for (var key in obj) {
    if ($Object.hasOwnProperty(obj, key))
      $Function.call(f, self, key, obj[key]);
  }
}

/**
 * Assuming |array_of_dictionaries| is structured like this:
 * [{id: 1, ... }, {id: 2, ...}, ...], you can use
 * lookup(array_of_dictionaries, 'id', 2) to get the dictionary with id == 2.
 * @param {Array<Object<?>>} array_of_dictionaries
 * @param {string} field
 * @param {?} value
 */
function lookup(array_of_dictionaries, field, value) {
  var filter = function (dict) {return dict[field] == value;};
  var matches = $Array.filter(array_of_dictionaries, filter);
  if (matches.length == 0) {
    return undefined;
  } else if (matches.length == 1) {
    return matches[0]
  } else {
    throw new Error("Failed lookup of field '" + field + "' with value '" +
                    value + "'");
  }
}

function loadTypeSchema(typeName, defaultSchema) {
  var parts = $String.split(typeName, '.');
  if (parts.length == 1) {
    if (defaultSchema == null) {
      WARNING('Trying to reference "' + typeName + '" ' +
              'with neither namespace nor default schema.');
      return null;
    }
    var types = defaultSchema.types;
  } else {
    var schemaName = $Array.join($Array.slice(parts, 0, parts.length - 1), '.');
    var types = schemaRegistry.GetSchema(schemaName).types;
  }
  for (var i = 0; i < types.length; ++i) {
    if (types[i].id == typeName)
      return types[i];
  }
  return null;
}

/**
 * Sets a property |value| on |obj| with property name |key|. Like
 *
 *     obj[key] = value;
 *
 * but without triggering setters.
 */
function defineProperty(obj, key, value) {
  $Object.defineProperty(obj, key, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    writable: true,
    value: value,
  });
}

/**
 * Takes a private class implementation |privateClass| and exposes a subset of
 * its methods |functions| and properties |properties| and |readonly| to a
 * public wrapper class that should be passed in. Within bindings code, you can
 * access the implementation from an instance of the wrapper class using
 * privates(instance).impl, and from the implementation class you can access
 * the wrapper using this.wrapper (or implInstance.wrapper if you have another
 * instance of the implementation class).
 *
 * |publicClass| should be a constructor that calls constructPrivate() like so:
 *
 *     privates(publicClass).constructPrivate(this, arguments);
 *
 * @param {function} publicClass The publicly exposed wrapper class. This must
 *     be a named function, and the name appears in stack traces.
 * @param {Object} privateClass The class implementation.
 * @param {{superclass: ?Function,
 *          functions: ?Array<string>,
 *          properties: ?Array<string>,
 *          readonly: ?Array<string>}} exposed The names of properties on the
 *     implementation class to be exposed. |superclass| represents the
 *     constructor of the class to be used as the superclass of the exposed
 *     class; |functions| represents the names of functions which should be
 *     delegated to the implementation; |properties| are gettable/settable
 *     properties and |readonly| are read-only properties.
 */
function expose(publicClass, privateClass, exposed) {
  $Object.setPrototypeOf(exposed, null);

  // This should be called by publicClass.
  privates(publicClass).constructPrivate = function(self, args) {
    if (!(self instanceof publicClass)) {
      throw new Error('Please use "new ' + publicClass.name + '"');
    }
    // The "instanceof publicClass" check can easily be spoofed, so we check
    // whether the private impl is already set before continuing.
    var privateSelf = privates(self);
    if ('impl' in privateSelf) {
      throw new Error('Object ' + publicClass.name + ' is already constructed');
    }
    var privateObj = $Object.create(privateClass.prototype);
    $Function.apply(privateClass, privateObj, args);
    privateObj.wrapper = self;
    privateSelf.impl = privateObj;
  };

  function getPrivateImpl(self) {
    var impl = privates(self).impl;
    if (!(impl instanceof privateClass)) {
      // Either the object is not constructed, or the property descriptor is
      // used on a target that is not an instance of publicClass.
      throw new Error('impl is not an instance of ' + privateClass.name);
    }
    return impl;
  }

  var publicClassPrototype = {
    // The final prototype will be assigned at the end of this method.
    __proto__: null,
    constructor: publicClass,
  };

  if ('functions' in exposed) {
    $Array.forEach(exposed.functions, function(func) {
      publicClassPrototype[func] = function() {
        var impl = getPrivateImpl(this);
        return $Function.apply(impl[func], impl, arguments);
      };
    });
  }

  if ('properties' in exposed) {
    $Array.forEach(exposed.properties, function(prop) {
      $Object.defineProperty(publicClassPrototype, prop, {
        __proto__: null,
        enumerable: true,
        get: function() {
          return getPrivateImpl(this)[prop];
        },
        set: function(value) {
          var impl = getPrivateImpl(this);
          delete impl[prop];
          impl[prop] = value;
        }
      });
    });
  }

  if ('readonly' in exposed) {
    $Array.forEach(exposed.readonly, function(readonly) {
      $Object.defineProperty(publicClassPrototype, readonly, {
        __proto__: null,
        enumerable: true,
        get: function() {
          return getPrivateImpl(this)[readonly];
        },
      });
    });
  }

  // The prototype properties have been installed. Now we can safely assign an
  // unsafe prototype and export the class to the public.
  var superclass = exposed.superclass || $Object.self;
  $Object.setPrototypeOf(publicClassPrototype, superclass.prototype);
  publicClass.prototype = publicClassPrototype;

  return publicClass;
}

/**
 * Returns a deep copy of |value|. The copy will have no references to nested
 * values of |value|.
 */
function deepCopy(value) {
  return nativeDeepCopy(value);
}

/**
 * Wrap an asynchronous API call to a function |func| in a promise. The
 * remaining arguments will be passed to |func|. Returns a promise that will be
 * resolved to the result passed to the callback or rejected if an error occurs
 * (if chrome.runtime.lastError is set). If there are multiple results, the
 * promise will be resolved with an array containing those results.
 *
 * For example,
 * promise(chrome.storage.get, 'a').then(function(result) {
 *   // Use result.
 * }).catch(function(error) {
 *   // Report error.message.
 * });
 */
function promise(func) {
  var args = $Array.slice(arguments, 1);
  DCHECK(typeof func == 'function');
  return new Promise(function(resolve, reject) {
    args.push(function() {
      if (chrome.runtime.lastError) {
        reject(new Error(chrome.runtime.lastError));
        return;
      }
      if (arguments.length <= 1)
        resolve(arguments[0]);
      else
        resolve($Array.slice(arguments));
    });
    $Function.apply(func, null, args);
  });
}

exports.$set('forEach', forEach);
exports.$set('loadTypeSchema', loadTypeSchema);
exports.$set('lookup', lookup);
exports.$set('defineProperty', defineProperty);
exports.$set('expose', expose);
exports.$set('deepCopy', deepCopy);
exports.$set('promise', promise);
