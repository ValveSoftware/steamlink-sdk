/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * FIXME: ES5 strict mode check is suppressed due to multiple uses of arguments.callee.
 * @fileoverview
 * @suppress {es5Strict}
 */

/**
 * @param {InjectedScriptHostClass} InjectedScriptHost
 * @param {Window} inspectedWindow
 * @param {number} injectedScriptId
 * @param {!InjectedScript} injectedScript
 */
(function (InjectedScriptHost, inspectedWindow, injectedScriptId, injectedScript) {

var TypeUtils = {
    /**
     * http://www.khronos.org/registry/typedarray/specs/latest/#7
     * @const
     * @type {!Array.<function(new:ArrayBufferView, (!ArrayBuffer|!ArrayBufferView), number=, number=)>}
     */
    _typedArrayClasses: (function(typeNames) {
        var result = [];
        for (var i = 0, n = typeNames.length; i < n; ++i) {
            if (inspectedWindow[typeNames[i]])
                result.push(inspectedWindow[typeNames[i]]);
        }
        return result;
    })(["Int8Array", "Uint8Array", "Uint8ClampedArray", "Int16Array", "Uint16Array", "Int32Array", "Uint32Array", "Float32Array", "Float64Array"]),

    /**
     * @const
     * @type {!Array.<string>}
     */
    _supportedPropertyPrefixes: ["webkit"],

    /**
     * @param {*} array
     * @return {function(new:ArrayBufferView, (!ArrayBuffer|!ArrayBufferView), number=, number=)|null}
     */
    typedArrayClass: function(array)
    {
        var classes = TypeUtils._typedArrayClasses;
        for (var i = 0, n = classes.length; i < n; ++i) {
            if (array instanceof classes[i])
                return classes[i];
        }
        return null;
    },

    /**
     * @param {*} obj
     * @return {*}
     */
    clone: function(obj)
    {
        if (!obj)
            return obj;

        var type = typeof obj;
        if (type !== "object" && type !== "function")
            return obj;

        // Handle Array and ArrayBuffer instances.
        if (typeof obj.slice === "function") {
            console.assert(obj instanceof Array || obj instanceof ArrayBuffer);
            return obj.slice(0);
        }

        var typedArrayClass = TypeUtils.typedArrayClass(obj);
        if (typedArrayClass)
            return new typedArrayClass(/** @type {!ArrayBufferView} */ (obj));

        if (obj instanceof HTMLImageElement) {
            var img = /** @type {!HTMLImageElement} */ (obj);
            // Special case for Images with Blob URIs: cloneNode will fail if the Blob URI has already been revoked.
            // FIXME: Maybe this is a bug in WebKit core?
            if (/^blob:/.test(img.src))
                return TypeUtils.cloneIntoCanvas(img);
            return img.cloneNode(true);
        }

        if (obj instanceof HTMLCanvasElement)
            return TypeUtils.cloneIntoCanvas(obj);

        if (obj instanceof HTMLVideoElement)
            return TypeUtils.cloneIntoCanvas(obj, obj.videoWidth, obj.videoHeight);

        if (obj instanceof ImageData) {
            var context = TypeUtils._dummyCanvas2dContext();
            // FIXME: suppress type checks due to outdated builtin externs for createImageData.
            var result = (/** @type {?} */ (context)).createImageData(obj);
            for (var i = 0, n = obj.data.length; i < n; ++i)
              result.data[i] = obj.data[i];
            return result;
        }

        // Try to convert to a primitive value via valueOf().
        if (typeof obj.valueOf === "function") {
            var value = obj.valueOf();
            var valueType = typeof value;
            if (valueType !== "object" && valueType !== "function")
                return value;
        }

        console.error("ASSERT_NOT_REACHED: failed to clone object: ", obj);
        return obj;
    },

    /**
     * @param {!HTMLImageElement|!HTMLCanvasElement|!HTMLVideoElement} obj
     * @param {number=} width
     * @param {number=} height
     * @return {!HTMLCanvasElement}
     */
    cloneIntoCanvas: function(obj, width, height)
    {
        var canvas = /** @type {!HTMLCanvasElement} */ (inspectedWindow.document.createElement("canvas"));
        canvas.width = width || +obj.width;
        canvas.height = height || +obj.height;
        var context = /** @type {!CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d")));
        context.drawImage(obj, 0, 0);
        return canvas;
    },

    /**
     * @param {?Object=} obj
     * @return {?Object}
     */
    cloneObject: function(obj)
    {
        if (!obj)
            return null;
        var result = {};
        for (var key in obj)
            result[key] = obj[key];
        return result;
    },

    /**
     * @param {!Array.<string>} names
     * @return {!Object.<string, boolean>}
     */
    createPrefixedPropertyNamesSet: function(names)
    {
        var result = Object.create(null);
        for (var i = 0, name; name = names[i]; ++i) {
            result[name] = true;
            var suffix = name.substr(0, 1).toUpperCase() + name.substr(1);
            for (var j = 0, prefix; prefix = TypeUtils._supportedPropertyPrefixes[j]; ++j)
                result[prefix + suffix] = true;
        }
        return result;
    },

    /**
     * @return {number}
     */
    now: function()
    {
        try {
            return inspectedWindow.performance.now();
        } catch(e) {
            try {
                return Date.now();
            } catch(ex) {
            }
        }
        return 0;
    },

    /**
     * @param {string} property
     * @param {!Object} obj
     * @return {boolean}
     */
    isEnumPropertyName: function(property, obj)
    {
        return (/^[A-Z][A-Z0-9_]+$/.test(property) && typeof obj[property] === "number");
    },

    /**
     * @return {!CanvasRenderingContext2D}
     */
    _dummyCanvas2dContext: function()
    {
        var context = TypeUtils._dummyCanvas2dContextInstance;
        if (!context) {
            var canvas = /** @type {!HTMLCanvasElement} */ (inspectedWindow.document.createElement("canvas"));
            context = /** @type {!CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d")));
            TypeUtils._dummyCanvas2dContextInstance = context;
        }
        return context;
    }
}

/** @typedef {{name:string, valueIsEnum:(boolean|undefined), value:*, values:(!Array.<!TypeUtils.InternalResourceStateDescriptor>|undefined), isArray:(boolean|undefined)}} */
TypeUtils.InternalResourceStateDescriptor;

/**
 * @interface
 */
function StackTrace()
{
}

StackTrace.prototype = {
    /**
     * @param {number} index
     * @return {{sourceURL: string, lineNumber: number, columnNumber: number}|undefined}
     */
    callFrame: function(index)
    {
    }
}

/**
 * @param {number=} stackTraceLimit
 * @param {?Function=} topMostFunctionToIgnore
 * @return {?StackTrace}
 */
StackTrace.create = function(stackTraceLimit, topMostFunctionToIgnore)
{
    if (typeof Error.captureStackTrace === "function")
        return new StackTraceV8(stackTraceLimit, topMostFunctionToIgnore || arguments.callee);
    // FIXME: Support JSC, and maybe other browsers.
    return null;
}

/**
 * @constructor
 * @implements {StackTrace}
 * @param {number=} stackTraceLimit
 * @param {?Function=} topMostFunctionToIgnore
 * @see http://code.google.com/p/v8/wiki/JavaScriptStackTraceApi
 */
function StackTraceV8(stackTraceLimit, topMostFunctionToIgnore)
{
    var oldPrepareStackTrace = Error.prepareStackTrace;
    var oldStackTraceLimit = Error.stackTraceLimit;
    if (typeof stackTraceLimit === "number")
        Error.stackTraceLimit = stackTraceLimit;

    /**
     * @param {!Object} error
     * @param {!Array.<!CallSite>} structuredStackTrace
     * @return {!Array.<{sourceURL: string, lineNumber: number, columnNumber: number}>}
     */
    Error.prepareStackTrace = function(error, structuredStackTrace)
    {
        return structuredStackTrace.map(function(callSite) {
            return {
                sourceURL: callSite.getFileName(),
                lineNumber: callSite.getLineNumber(),
                columnNumber: callSite.getColumnNumber()
            };
        });
    }

    var holder = /** @type {{stack: !Array.<{sourceURL: string, lineNumber: number, columnNumber: number}>}} */ ({});
    Error.captureStackTrace(holder, topMostFunctionToIgnore || arguments.callee);
    this._stackTrace = holder.stack;

    Error.stackTraceLimit = oldStackTraceLimit;
    Error.prepareStackTrace = oldPrepareStackTrace;
}

StackTraceV8.prototype = {
    /**
     * @override
     * @param {number} index
     * @return {{sourceURL: string, lineNumber: number, columnNumber: number}|undefined}
     */
    callFrame: function(index)
    {
        return this._stackTrace[index];
    }
}

/**
 * @constructor
 * @template T
 */
function Cache()
{
    this.reset();
}

Cache.prototype = {
    /**
     * @return {number}
     */
    size: function()
    {
        return this._size;
    },

    reset: function()
    {
        /** @type {!Object.<number, !T>} */
        this._items = Object.create(null);
        /** @type {number} */
        this._size = 0;
    },

    /**
     * @param {number} key
     * @return {boolean}
     */
    has: function(key)
    {
        return key in this._items;
    },

    /**
     * @param {number} key
     * @return {T|undefined}
     */
    get: function(key)
    {
        return this._items[key];
    },

    /**
     * @param {number} key
     * @param {!T} item
     */
    put: function(key, item)
    {
        if (!this.has(key))
            ++this._size;
        this._items[key] = item;
    }
}

/**
 * @constructor
 * @param {?Resource|!Object} thisObject
 * @param {string} functionName
 * @param {!Array|!Arguments} args
 * @param {!Resource|*=} result
 * @param {?StackTrace=} stackTrace
 */
function Call(thisObject, functionName, args, result, stackTrace)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = Array.prototype.slice.call(args, 0);
    this._result = result;
    this._stackTrace = stackTrace || null;

    if (!this._functionName)
        console.assert(this._args.length === 2 && typeof this._args[0] === "string");
}

Call.prototype = {
    /**
     * @return {?Resource}
     */
    resource: function()
    {
        return Resource.forObject(this._thisObject);
    },

    /**
     * @return {string}
     */
    functionName: function()
    {
        return this._functionName;
    },

    /**
     * @return {boolean}
     */
    isPropertySetter: function()
    {
        return !this._functionName;
    },
    
    /**
     * @return {!Array}
     */
    args: function()
    {
        return this._args;
    },

    /**
     * @return {*}
     */
    result: function()
    {
        return this._result;
    },

    /**
     * @return {?StackTrace}
     */
    stackTrace: function()
    {
        return this._stackTrace;
    },

    /**
     * @param {?StackTrace} stackTrace
     */
    setStackTrace: function(stackTrace)
    {
        this._stackTrace = stackTrace;
    },

    /**
     * @param {*} result
     */
    setResult: function(result)
    {
        this._result = result;
    },

    /**
     * @param {string} name
     * @param {?Object} attachment
     */
    setAttachment: function(name, attachment)
    {
        if (attachment) {
            /** @type {?Object.<string, !Object>|undefined} */
            this._attachments = this._attachments || Object.create(null);
            this._attachments[name] = attachment;
        } else if (this._attachments) {
            delete this._attachments[name];
        }
    },

    /**
     * @param {string} name
     * @return {?Object}
     */
    attachment: function(name)
    {
        return this._attachments ? (this._attachments[name] || null) : null;
    },

    freeze: function()
    {
        if (this._freezed)
            return;
        this._freezed = true;
        for (var i = 0, n = this._args.length; i < n; ++i) {
            // FIXME: freeze the Resources also!
            if (!Resource.forObject(this._args[i]))
                this._args[i] = TypeUtils.clone(this._args[i]);
        }
    },

    /**
     * @param {!Cache.<!ReplayableResource>} cache
     * @return {!ReplayableCall}
     */
    toReplayable: function(cache)
    {
        this.freeze();
        var thisObject = /** @type {!ReplayableResource} */ (Resource.toReplayable(this._thisObject, cache));
        var result = Resource.toReplayable(this._result, cache);
        var args = this._args.map(function(obj) {
            return Resource.toReplayable(obj, cache);
        });
        var attachments = TypeUtils.cloneObject(this._attachments);
        return new ReplayableCall(thisObject, this._functionName, args, result, this._stackTrace, attachments);
    },

    /**
     * @param {!ReplayableCall} replayableCall
     * @param {!Cache.<!Resource>} cache
     * @return {!Call}
     */
    replay: function(replayableCall, cache)
    {
        var replayableResult = replayableCall.result();
        if (replayableResult instanceof ReplayableResource && !cache.has(replayableResult.id())) {
            var resource = replayableResult.replay(cache);
            console.assert(resource.calls().length > 0, "Expected create* call for the Resource");
            return resource.calls()[0];
        }

        var replayObject = ReplayableResource.replay(replayableCall.replayableResource(), cache);
        var replayArgs = replayableCall.args().map(function(obj) {
            return ReplayableResource.replay(obj, cache);
        });
        var replayResult = undefined;

        if (replayableCall.isPropertySetter())
            replayObject[replayArgs[0]] = replayArgs[1];
        else {
            var replayFunction = replayObject[replayableCall.functionName()];
            console.assert(typeof replayFunction === "function", "Expected a function to replay");
            replayResult = replayFunction.apply(replayObject, replayArgs);

            if (replayableResult instanceof ReplayableResource) {
                var resource = replayableResult.replay(cache);
                if (!resource.wrappedObject())
                    resource.setWrappedObject(replayResult);
            }
        }
    
        this._thisObject = replayObject;
        this._functionName = replayableCall.functionName();
        this._args = replayArgs;
        this._result = replayResult;
        this._stackTrace = replayableCall.stackTrace();
        this._freezed = true;
        var attachments = replayableCall.attachments();
        this._attachments = attachments ? TypeUtils.cloneObject(attachments) : null;
        return this;
    }
}

/**
 * @constructor
 * @param {!ReplayableResource} thisObject
 * @param {string} functionName
 * @param {!Array.<!ReplayableResource|*>} args
 * @param {!ReplayableResource|*} result
 * @param {?StackTrace} stackTrace
 * @param {?Object.<string, !Object>} attachments
 */
function ReplayableCall(thisObject, functionName, args, result, stackTrace, attachments)
{
    this._thisObject = thisObject;
    this._functionName = functionName;
    this._args = args;
    this._result = result;
    this._stackTrace = stackTrace;
    if (attachments)
        this._attachments = attachments;
}

ReplayableCall.prototype = {
    /**
     * @return {!ReplayableResource}
     */
    replayableResource: function()
    {
        return this._thisObject;
    },

    /**
     * @return {string}
     */
    functionName: function()
    {
        return this._functionName;
    },

    /**
     * @return {boolean}
     */
    isPropertySetter: function()
    {
        return !this._functionName;
    },

    /**
     * @return {string}
     */
    propertyName: function()
    {
        console.assert(this.isPropertySetter());
        return /** @type {string} */ (this._args[0]);
    },

    /**
     * @return {*}
     */
    propertyValue: function()
    {
        console.assert(this.isPropertySetter());
        return this._args[1];
    },

    /**
     * @return {!Array.<!ReplayableResource|*>}
     */
    args: function()
    {
        return this._args;
    },

    /**
     * @return {!ReplayableResource|*}
     */
    result: function()
    {
        return this._result;
    },

    /**
     * @return {?StackTrace}
     */
    stackTrace: function()
    {
        return this._stackTrace;
    },

    /**
     * @return {?Object.<string, !Object>}
     */
    attachments: function()
    {
        return this._attachments;
    },

    /**
     * @param {string} name
     * @return {!Object}
     */
    attachment: function(name)
    {
        return this._attachments && this._attachments[name];
    },

    /**
     * @param {!Cache.<!Resource>} cache
     * @return {!Call}
     */
    replay: function(cache)
    {
        var call = /** @type {!Call} */ (Object.create(Call.prototype));
        return call.replay(this, cache);
    }
}

/**
 * @constructor
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function Resource(wrappedObject, name)
{
    /** @type {number} */
    this._id = ++Resource._uniqueId;
    /** @type {string} */
    this._name = name || "Resource";
    /** @type {number} */
    this._kindId = Resource._uniqueKindIds[this._name] = (Resource._uniqueKindIds[this._name] || 0) + 1;
    /** @type {?ResourceTrackingManager} */
    this._resourceManager = null;
    /** @type {!Array.<!Call>} */
    this._calls = [];
    /**
     * This is to prevent GC from collecting associated resources.
     * Otherwise, for example in WebGL, subsequent calls to gl.getParameter()
     * may return a recently created instance that is no longer bound to a
     * Resource object (thus, no history to replay it later).
     *
     * @type {!Object.<string, !Resource>}
     */
    this._boundResources = Object.create(null);
    this.setWrappedObject(wrappedObject);
}

/**
 * @type {number}
 */
Resource._uniqueId = 0;

/**
 * @type {!Object.<string, number>}
 */
Resource._uniqueKindIds = {};

/**
 * @param {*} obj
 * @return {?Resource}
 */
Resource.forObject = function(obj)
{
    if (!obj)
        return null;
    if (obj instanceof Resource)
        return obj;
    if (typeof obj === "object")
        return obj["__resourceObject"];
    return null;
}

/**
 * @param {!Resource|*} obj
 * @return {*}
 */
Resource.wrappedObject = function(obj)
{
    var resource = Resource.forObject(obj);
    return resource ? resource.wrappedObject() : obj;
}

/**
 * @param {!Resource|*} obj
 * @param {!Cache.<!ReplayableResource>} cache
 * @return {!ReplayableResource|*}
 */
Resource.toReplayable = function(obj, cache)
{
    var resource = Resource.forObject(obj);
    return resource ? resource.toReplayable(cache) : obj;
}

Resource.prototype = {
    /**
     * @return {number}
     */
    id: function()
    {
        return this._id;
    },

    /**
     * @return {string}
     */
    name: function()
    {
        return this._name;
    },

    /**
     * @return {string}
     */
    description: function()
    {
        return this._name + "@" + this._kindId;
    },

    /**
     * @return {!Object}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @param {!Object} value
     */
    setWrappedObject: function(value)
    {
        console.assert(value, "wrappedObject should not be NULL");
        console.assert(!(value instanceof Resource), "Binding a Resource object to another Resource object?");
        this._wrappedObject = value;
        this._bindObjectToResource(value);
    },

    /**
     * @return {!Object}
     */
    proxyObject: function()
    {
        if (!this._proxyObject)
            this._proxyObject = this._wrapObject();
        return this._proxyObject;
    },

    /**
     * @return {?ResourceTrackingManager}
     */
    manager: function()
    {
        return this._resourceManager;
    },

    /**
     * @param {!ResourceTrackingManager} value
     */
    setManager: function(value)
    {
        this._resourceManager = value;
    },

    /**
     * @return {!Array.<!Call>}
     */
    calls: function()
    {
        return this._calls;
    },

    /**
     * @return {?ContextResource}
     */
    contextResource: function()
    {
        if (this instanceof ContextResource)
            return /** @type {!ContextResource} */ (this);

        if (this._calculatingContextResource)
            return null;

        this._calculatingContextResource = true;
        var result = null;
        for (var i = 0, n = this._calls.length; i < n; ++i) {
            result = this._calls[i].resource().contextResource();
            if (result)
                break;
        }
        delete this._calculatingContextResource;
        console.assert(result, "Failed to find context resource for " + this._name + "@" + this._kindId);
        return result;
    },

    /**
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var proxyObject = this.proxyObject();
        if (!proxyObject)
            return result;
        var statePropertyNames = this._proxyStatePropertyNames || [];
        for (var i = 0, n = statePropertyNames.length; i < n; ++i) {
            var pname = statePropertyNames[i];
            result.push({ name: pname, value: proxyObject[pname] });
        }
        result.push({ name: "context", value: this.contextResource() });
        return result;
    },

    /**
     * @return {string}
     */
    toDataURL: function()
    {
        return "";
    },

    /**
     * @param {!Cache.<!ReplayableResource>} cache
     * @return {!ReplayableResource}
     */
    toReplayable: function(cache)
    {
        var result = cache.get(this._id);
        if (result)
            return result;
        var data = {
            id: this._id,
            name: this._name,
            kindId: this._kindId
        };
        result = new ReplayableResource(this, data);
        cache.put(this._id, result); // Put into the cache early to avoid loops.
        data.calls = this._calls.map(function(call) {
            return call.toReplayable(cache);
        });
        this._populateReplayableData(data, cache);
        var contextResource = this.contextResource();
        if (contextResource !== this)
            data.contextResource = Resource.toReplayable(contextResource, cache);
        return result;
    },

    /**
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        // Do nothing. Should be overridden by subclasses.
    },

    /**
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     * @return {!Resource}
     */
    replay: function(data, cache)
    {
        var resource = cache.get(data.id);
        if (resource)
            return resource;
        this._id = data.id;
        this._name = data.name;
        this._kindId = data.kindId;
        this._resourceManager = null;
        this._calls = [];
        this._boundResources = Object.create(null);
        this._wrappedObject = null;
        cache.put(data.id, this); // Put into the cache early to avoid loops.
        this._doReplayCalls(data, cache);
        console.assert(this._wrappedObject, "Resource should be reconstructed!");
        return this;
    },

    /**
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        for (var i = 0, n = data.calls.length; i < n; ++i)
            this._calls.push(data.calls[i].replay(cache));
    },

    /**
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        call.freeze();
        this._calls.push(call);
    },

    /**
     * @param {!Call} call
     */
    onCallReplayed: function(call)
    {
        // Ignore by default.
    },

    /**
     * @param {!Object} object
     */
    _bindObjectToResource: function(object)
    {
        Object.defineProperty(object, "__resourceObject", {
            value: this,
            writable: false,
            enumerable: false,
            configurable: true
        });
    },

    /**
     * @param {string} key
     * @param {*} obj
     */
    _registerBoundResource: function(key, obj)
    {
        var resource = Resource.forObject(obj);
        if (resource)
            this._boundResources[key] = resource;
        else
            delete this._boundResources[key];
    },

    /**
     * @return {?Object}
     */
    _wrapObject: function()
    {
        var wrappedObject = this.wrappedObject();
        if (!wrappedObject)
            return null;
        var proxy = Object.create(wrappedObject.__proto__); // In order to emulate "instanceof".

        var customWrapFunctions = this._customWrapFunctions();
        /** @type {!Array.<string>} */
        this._proxyStatePropertyNames = [];

        /**
         * @param {string} property
         * @this {Resource}
         */
        function processProperty(property)
        {
            if (typeof wrappedObject[property] === "function") {
                var customWrapFunction = customWrapFunctions[property];
                if (customWrapFunction)
                    proxy[property] = this._wrapCustomFunction(this, wrappedObject, wrappedObject[property], property, customWrapFunction);
                else
                    proxy[property] = this._wrapFunction(this, wrappedObject, wrappedObject[property], property);
            } else if (TypeUtils.isEnumPropertyName(property, wrappedObject)) {
                // Fast access to enums and constants.
                proxy[property] = wrappedObject[property];
            } else {
                this._proxyStatePropertyNames.push(property);
                Object.defineProperty(proxy, property, {
                    get: function()
                    {
                        var obj = wrappedObject[property];
                        var resource = Resource.forObject(obj);
                        return resource ? resource : obj;
                    },
                    set: this._wrapPropertySetter(this, wrappedObject, property),
                    enumerable: true
                });
            }
        }

        var isEmpty = true;
        for (var property in wrappedObject) {
            isEmpty = false;
            processProperty.call(this, property);
        }
        if (isEmpty)
            return wrappedObject; // Nothing to proxy.

        this._bindObjectToResource(proxy);
        return proxy;
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {!Function} originalFunction
     * @param {string} functionName
     * @param {!Function} customWrapFunction
     * @return {!Function}
     */
    _wrapCustomFunction: function(resource, originalObject, originalFunction, functionName, customWrapFunction)
    {
        return function()
        {
            var manager = resource.manager();
            var isCapturing = manager && manager.capturing();
            if (isCapturing)
                manager.captureArguments(resource, arguments);
            var wrapFunction = new Resource.WrapFunction(originalObject, originalFunction, functionName, arguments);
            customWrapFunction.apply(wrapFunction, arguments);
            if (isCapturing) {
                var call = wrapFunction.call();
                call.setStackTrace(StackTrace.create(1, arguments.callee));
                manager.captureCall(call);
            }
            return wrapFunction.result();
        };
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {!Function} originalFunction
     * @param {string} functionName
     * @return {!Function}
     */
    _wrapFunction: function(resource, originalObject, originalFunction, functionName)
    {
        return function()
        {
            var manager = resource.manager();
            if (!manager || !manager.capturing())
                return originalFunction.apply(originalObject, arguments);
            manager.captureArguments(resource, arguments);
            var result = originalFunction.apply(originalObject, arguments);
            var stackTrace = StackTrace.create(1, arguments.callee);
            var call = new Call(resource, functionName, arguments, result, stackTrace);
            manager.captureCall(call);
            return result;
        };
    },

    /**
     * @param {!Resource} resource
     * @param {!Object} originalObject
     * @param {string} propertyName
     * @return {function(*)}
     */
    _wrapPropertySetter: function(resource, originalObject, propertyName)
    {
        return function(value)
        {
            resource._registerBoundResource(propertyName, value);
            var manager = resource.manager();
            if (!manager || !manager.capturing()) {
                originalObject[propertyName] = Resource.wrappedObject(value);
                return;
            }
            var args = [propertyName, value];
            manager.captureArguments(resource, args);
            originalObject[propertyName] = Resource.wrappedObject(value);
            var stackTrace = StackTrace.create(1, arguments.callee);
            var call = new Call(resource, "", args, undefined, stackTrace);
            manager.captureCall(call);
        };
    },

    /**
     * @return {!Object.<string, !Function>}
     */
    _customWrapFunctions: function()
    {
        return Object.create(null); // May be overridden by subclasses.
    }
}

/**
 * @constructor
 * @param {!Object} originalObject
 * @param {!Function} originalFunction
 * @param {string} functionName
 * @param {!Array|!Arguments} args
 */
Resource.WrapFunction = function(originalObject, originalFunction, functionName, args)
{
    this._originalObject = originalObject;
    this._originalFunction = originalFunction;
    this._functionName = functionName;
    this._args = args;
    this._resource = Resource.forObject(originalObject);
    console.assert(this._resource, "Expected a wrapped call on a Resource object.");
}

Resource.WrapFunction.prototype = {
    /**
     * @return {*}
     */
    result: function()
    {
        if (!this._executed) {
            this._executed = true;
            this._result = this._originalFunction.apply(this._originalObject, this._args);
        }
        return this._result;
    },

    /**
     * @return {!Call}
     */
    call: function()
    {
        if (!this._call)
            this._call = new Call(this._resource, this._functionName, this._args, this.result());
        return this._call;
    },

    /**
     * @param {*} result
     */
    overrideResult: function(result)
    {
        var call = this.call();
        call.setResult(result);
        this._result = result;
    }
}

/**
 * @param {function(new:Resource, !Object, string)} resourceConstructor
 * @param {string} resourceName
 * @return {function(this:Resource.WrapFunction)}
 */
Resource.WrapFunction.resourceFactoryMethod = function(resourceConstructor, resourceName)
{
    /** @this {Resource.WrapFunction} */
    return function()
    {
        var wrappedObject = /** @type {?Object} */ (this.result());
        if (!wrappedObject)
            return;
        var resource = new resourceConstructor(wrappedObject, resourceName);
        var manager = this._resource.manager();
        if (manager)
            manager.registerResource(resource);
        this.overrideResult(resource.proxyObject());
        resource.pushCall(this.call());
    }
}

/**
 * @constructor
 * @param {!Resource} originalResource
 * @param {!Object} data
 */
function ReplayableResource(originalResource, data)
{
    this._proto = originalResource.__proto__;
    this._data = data;
}

ReplayableResource.prototype = {
    /**
     * @return {number}
     */
    id: function()
    {
        return this._data.id;
    },

    /**
     * @return {string}
     */
    name: function()
    {
        return this._data.name;
    },

    /**
     * @return {string}
     */
    description: function()
    {
        return this._data.name + "@" + this._data.kindId;
    },

    /**
     * @return {!ReplayableResource}
     */
    contextResource: function()
    {
        return this._data.contextResource || this;
    },

    /**
     * @param {!Cache.<!Resource>} cache
     * @return {!Resource}
     */
    replay: function(cache)
    {
        var result = /** @type {!Resource} */ (Object.create(this._proto));
        result = result.replay(this._data, cache)
        console.assert(result.__proto__ === this._proto, "Wrong type of a replay result");
        return result;
    }
}

/**
 * @param {!ReplayableResource|*} obj
 * @param {!Cache.<!Resource>} cache
 * @return {*}
 */
ReplayableResource.replay = function(obj, cache)
{
    return (obj instanceof ReplayableResource) ? obj.replay(cache).wrappedObject() : obj;
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function ContextResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
}

ContextResource.prototype = {
    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function LogEverythingResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
}

LogEverythingResource.prototype = {
    /**
     * @override
     * @return {!Object.<string, !Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = Object.create(null);
        var wrappedObject = this.wrappedObject();
        if (wrappedObject) {
            for (var property in wrappedObject) {
                /** @this {Resource.WrapFunction} */
                wrapFunctions[property] = function()
                {
                    this._resource.pushCall(this.call());
                }
            }
        }
        return wrapFunctions;
    },

    __proto__: Resource.prototype
}

////////////////////////////////////////////////////////////////////////////////
// WebGL
////////////////////////////////////////////////////////////////////////////////

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLBoundResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
    /** @type {!Object.<string, *>} */
    this._state = {};
}

WebGLBoundResource.prototype = {
    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var state = this._state;
        data.state = {};
        Object.keys(state).forEach(function(parameter) {
            data.state[parameter] = Resource.toReplayable(state[parameter], cache);
        });
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        var gl = this._replayContextResource(data, cache).wrappedObject();

        /** @type {!Object.<string, !Array.<string>>} */
        var bindingsData = {
            TEXTURE_2D: ["bindTexture", "TEXTURE_BINDING_2D"],
            TEXTURE_CUBE_MAP: ["bindTexture", "TEXTURE_BINDING_CUBE_MAP"],
            ARRAY_BUFFER: ["bindBuffer", "ARRAY_BUFFER_BINDING"],
            ELEMENT_ARRAY_BUFFER: ["bindBuffer", "ELEMENT_ARRAY_BUFFER_BINDING"],
            FRAMEBUFFER: ["bindFramebuffer", "FRAMEBUFFER_BINDING"],
            RENDERBUFFER: ["bindRenderbuffer", "RENDERBUFFER_BINDING"]
        };
        var originalBindings = {};
        Object.keys(bindingsData).forEach(function(bindingTarget) {
            var bindingParameter = bindingsData[bindingTarget][1];
            originalBindings[bindingTarget] = gl.getParameter(gl[bindingParameter]);
        });

        var state = {};
        Object.keys(data.state).forEach(function(parameter) {
            state[parameter] = ReplayableResource.replay(data.state[parameter], cache);
        });
        this._state = state;
        Resource.prototype._doReplayCalls.call(this, data, cache);

        Object.keys(bindingsData).forEach(function(bindingTarget) {
            var bindMethodName = bindingsData[bindingTarget][0];
            gl[bindMethodName].call(gl, gl[bindingTarget], originalBindings[bindingTarget]);
        });
    },

    /**
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     * @return {?WebGLRenderingContextResource}
     */
    _replayContextResource: function(data, cache)
    {
        var calls = /** @type {!Array.<!ReplayableCall>} */ (data.calls);
        for (var i = 0, n = calls.length; i < n; ++i) {
            var resource = ReplayableResource.replay(calls[i].replayableResource(), cache);
            var contextResource = WebGLRenderingContextResource.forObject(resource);
            if (contextResource)
                return contextResource;
        }
        return null;
    },

    /**
     * @param {number} target
     * @param {string} bindMethodName
     */
    pushBinding: function(target, bindMethodName)
    {
        if (this._state.bindTarget !== target) {
            this._state.bindTarget = target;
            this.pushCall(new Call(WebGLRenderingContextResource.forObject(this), bindMethodName, [target, this]));
        }
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLTextureResource(wrappedObject, name)
{
    WebGLBoundResource.call(this, wrappedObject, name);
}

WebGLTextureResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLTexture}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var texture = this.wrappedObject();
        if (!gl || !texture)
            return result;
        result.push({ name: "isTexture", value: gl.isTexture(texture) });
        result.push({ name: "context", value: this.contextResource() });

        var target = this._state.bindTarget;
        if (typeof target !== "number")
            return result;

        var bindingParameter;
        switch (target) {
        case gl.TEXTURE_2D:
            bindingParameter = gl.TEXTURE_BINDING_2D;
            break;
        case gl.TEXTURE_CUBE_MAP:
            bindingParameter = gl.TEXTURE_BINDING_CUBE_MAP;
            break;
        default:
            console.error("ASSERT_NOT_REACHED: unknown texture target " + target);
            return result;
        }
        result.push({ name: "target", value: target, valueIsEnum: true });

        var oldTexture = /** @type {!WebGLTexture} */ (gl.getParameter(bindingParameter));
        if (oldTexture !== texture)
            gl.bindTexture(target, texture);

        var textureParameters = [
            "TEXTURE_MAG_FILTER",
            "TEXTURE_MIN_FILTER",
            "TEXTURE_WRAP_S",
            "TEXTURE_WRAP_T",
            "TEXTURE_MAX_ANISOTROPY_EXT" // EXT_texture_filter_anisotropic extension
        ];
        glResource.queryStateValues(gl.getTexParameter, target, textureParameters, result);

        if (oldTexture !== texture)
            gl.bindTexture(target, oldTexture);
        return result;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        var gl = this._replayContextResource(data, cache).wrappedObject();

        var state = {};
        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            state[parameter] = gl.getParameter(gl[parameter]);
        });

        WebGLBoundResource.prototype._doReplayCalls.call(this, data, cache);

        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            gl.pixelStorei(gl[parameter], state[parameter]);
        });
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        var gl = WebGLRenderingContextResource.forObject(call.resource()).wrappedObject();
        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            var value = gl.getParameter(gl[parameter]);
            if (this._state[parameter] !== value) {
                this._state[parameter] = value;
                var pixelStoreCall = new Call(gl, "pixelStorei", [gl[parameter], value]);
                WebGLBoundResource.prototype.pushCall.call(this, pixelStoreCall);
            }
        }, this);

        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: optimize memory usage: maybe it's more efficient to store one texImage2D call instead of many texSubImage2D.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    /**
     * Handles: texParameteri, texParameterf
     * @param {!Call} call
     */
    pushCall_texParameter: function(call)
    {
        var args = call.args();
        var pname = args[1];
        var param = args[2];
        if (this._state[pname] !== param) {
            this._state[pname] = param;
            WebGLBoundResource.prototype.pushCall.call(this, call);
        }
    },

    /**
     * Handles: copyTexImage2D, copyTexSubImage2D
     * copyTexImage2D and copyTexSubImage2D define a texture image with pixels from the current framebuffer.
     * @param {!Call} call
     */
    pushCall_copyTexImage2D: function(call)
    {
        var glResource = WebGLRenderingContextResource.forObject(call.resource());
        var gl = glResource.wrappedObject();
        var framebufferResource = /** @type {!WebGLFramebufferResource} */ (glResource.currentBinding(gl.FRAMEBUFFER));
        if (framebufferResource)
            this.pushCall(new Call(glResource, "bindFramebuffer", [gl.FRAMEBUFFER, framebufferResource]));
        else {
            // FIXME: Implement this case.
            console.error("ASSERT_NOT_REACHED: Could not properly process a gl." + call.functionName() + " call while the DRAWING BUFFER is bound.");
        }
        this.pushCall(call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLProgramResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
}

WebGLProgramResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLProgram}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        /**
         * @param {!Object} obj
         * @param {!Array.<!TypeUtils.InternalResourceStateDescriptor>} output
         */
        function convertToStateDescriptors(obj, output)
        {
            for (var pname in obj)
                output.push({ name: pname, value: obj[pname], valueIsEnum: (pname === "type") });
        }

        var result = [];
        var program = this.wrappedObject();
        if (!program)
            return result;
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var programParameters = ["DELETE_STATUS", "LINK_STATUS", "VALIDATE_STATUS"];
        glResource.queryStateValues(gl.getProgramParameter, program, programParameters, result);
        result.push({ name: "getProgramInfoLog", value: gl.getProgramInfoLog(program) });
        result.push({ name: "isProgram", value: gl.isProgram(program) });
        result.push({ name: "context", value: this.contextResource() });

        // ATTACHED_SHADERS
        var callFormatter = CallFormatter.forResource(this);
        var shaders = gl.getAttachedShaders(program) || [];
        var shaderDescriptors = [];
        for (var i = 0, n = shaders.length; i < n; ++i) {
            var shaderResource = Resource.forObject(shaders[i]);
            var pname = callFormatter.enumNameForValue(shaderResource.type());
            shaderDescriptors.push({ name: pname, value: shaderResource });
        }
        result.push({ name: "ATTACHED_SHADERS", values: shaderDescriptors, isArray: true });

        // ACTIVE_UNIFORMS
        var uniformDescriptors = [];
        var uniforms = this._activeUniforms(true);
        for (var i = 0, n = uniforms.length; i < n; ++i) {
            var pname = "" + i;
            var values = [];
            convertToStateDescriptors(uniforms[i], values);
            uniformDescriptors.push({ name: pname, values: values });
        }
        result.push({ name: "ACTIVE_UNIFORMS", values: uniformDescriptors, isArray: true });

        // ACTIVE_ATTRIBUTES
        var attributesCount = /** @type {number} */ (gl.getProgramParameter(program, gl.ACTIVE_ATTRIBUTES));
        var attributeDescriptors = [];
        for (var i = 0; i < attributesCount; ++i) {
            var activeInfo = gl.getActiveAttrib(program, i);
            if (!activeInfo)
                continue;
            var pname = "" + i;
            var values = [];
            convertToStateDescriptors(activeInfo, values);
            attributeDescriptors.push({ name: pname, values: values });
        }
        result.push({ name: "ACTIVE_ATTRIBUTES", values: attributeDescriptors, isArray: true });

        return result;
    },

    /**
     * @param {boolean=} includeAllInfo
     * @return {!Array.<{name:string, type:number, value:*, size:(number|undefined)}>}
     */
    _activeUniforms: function(includeAllInfo)
    {
        var uniforms = [];
        var program = this.wrappedObject();
        if (!program)
            return uniforms;

        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();
        var uniformsCount = /** @type {number} */ (gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS));
        for (var i = 0; i < uniformsCount; ++i) {
            var activeInfo = gl.getActiveUniform(program, i);
            if (!activeInfo)
                continue;
            var uniformLocation = gl.getUniformLocation(program, activeInfo.name);
            if (!uniformLocation)
                continue;
            var value = gl.getUniform(program, uniformLocation);
            var item = Object.create(null);
            item.name = activeInfo.name;
            item.type = activeInfo.type;
            item.value = value;
            if (includeAllInfo)
                item.size = activeInfo.size;
            uniforms.push(item);
        }
        return uniforms;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var glResource = WebGLRenderingContextResource.forObject(this);
        var originalErrors = glResource.getAllErrors();
        data.uniforms = this._activeUniforms();
        glResource.restoreErrors(originalErrors);
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        Resource.prototype._doReplayCalls.call(this, data, cache);
        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();
        var program = this.wrappedObject();

        var originalProgram = /** @type {!WebGLProgram} */ (gl.getParameter(gl.CURRENT_PROGRAM));
        var currentProgram = originalProgram;
        
        data.uniforms.forEach(function(uniform) {
            var uniformLocation = gl.getUniformLocation(program, uniform.name);
            if (!uniformLocation)
                return;
            if (currentProgram !== program) {
                currentProgram = program;
                gl.useProgram(program);
            }
            var methodName = this._uniformMethodNameByType(gl, uniform.type);
            if (methodName.indexOf("Matrix") === -1)
                gl[methodName].call(gl, uniformLocation, uniform.value);
            else
                gl[methodName].call(gl, uniformLocation, false, uniform.value);
        }.bind(this));

        if (currentProgram !== originalProgram)
            gl.useProgram(originalProgram);
    },

    /**
     * @param {!WebGLRenderingContext} gl
     * @param {number} type
     * @return {string}
     */
    _uniformMethodNameByType: function(gl, type)
    {
        var uniformMethodNames = WebGLProgramResource._uniformMethodNames;
        if (!uniformMethodNames) {
            uniformMethodNames = {};
            uniformMethodNames[gl.FLOAT] = "uniform1f";
            uniformMethodNames[gl.FLOAT_VEC2] = "uniform2fv";
            uniformMethodNames[gl.FLOAT_VEC3] = "uniform3fv";
            uniformMethodNames[gl.FLOAT_VEC4] = "uniform4fv";
            uniformMethodNames[gl.INT] = "uniform1i";
            uniformMethodNames[gl.BOOL] = "uniform1i";
            uniformMethodNames[gl.SAMPLER_2D] = "uniform1i";
            uniformMethodNames[gl.SAMPLER_CUBE] = "uniform1i";
            uniformMethodNames[gl.INT_VEC2] = "uniform2iv";
            uniformMethodNames[gl.BOOL_VEC2] = "uniform2iv";
            uniformMethodNames[gl.INT_VEC3] = "uniform3iv";
            uniformMethodNames[gl.BOOL_VEC3] = "uniform3iv";
            uniformMethodNames[gl.INT_VEC4] = "uniform4iv";
            uniformMethodNames[gl.BOOL_VEC4] = "uniform4iv";
            uniformMethodNames[gl.FLOAT_MAT2] = "uniformMatrix2fv";
            uniformMethodNames[gl.FLOAT_MAT3] = "uniformMatrix3fv";
            uniformMethodNames[gl.FLOAT_MAT4] = "uniformMatrix4fv";
            WebGLProgramResource._uniformMethodNames = uniformMethodNames;
        }
        console.assert(uniformMethodNames[type], "Unknown uniform type " + type);
        return uniformMethodNames[type];
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple attachShader && detachShader.
        Resource.prototype.pushCall.call(this, call);
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLShaderResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
}

WebGLShaderResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLShader}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @return {number}
     */
    type: function()
    {
        var call = this._calls[0];
        if (call && call.functionName() === "createShader")
            return call.args()[0];
        console.error("ASSERT_NOT_REACHED: Failed to restore shader type from the log.", call);
        return 0;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var shader = this.wrappedObject();
        if (!shader)
            return result;
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var shaderParameters = ["SHADER_TYPE", "DELETE_STATUS", "COMPILE_STATUS"];
        glResource.queryStateValues(gl.getShaderParameter, shader, shaderParameters, result);
        result.push({ name: "getShaderInfoLog", value: gl.getShaderInfoLog(shader) });
        result.push({ name: "getShaderSource", value: gl.getShaderSource(shader) });
        result.push({ name: "isShader", value: gl.isShader(shader) });
        result.push({ name: "context", value: this.contextResource() });

        // getShaderPrecisionFormat
        var shaderType = this.type();
        var precisionValues = [];
        var precisionParameters = ["LOW_FLOAT", "MEDIUM_FLOAT", "HIGH_FLOAT", "LOW_INT", "MEDIUM_INT", "HIGH_INT"];
        for (var i = 0, pname; pname = precisionParameters[i]; ++i)
            precisionValues.push({ name: pname, value: gl.getShaderPrecisionFormat(shaderType, gl[pname]) });
        result.push({ name: "getShaderPrecisionFormat", values: precisionValues });

        return result;
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        // FIXME: handle multiple shaderSource calls.
        Resource.prototype.pushCall.call(this, call);
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLBufferResource(wrappedObject, name)
{
    WebGLBoundResource.call(this, wrappedObject, name);
}

WebGLBufferResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLBuffer}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @return {?ArrayBufferView}
     */
    cachedBufferData: function()
    {
        /**
         * Creates a view to a given buffer, does NOT copy the buffer.
         * @param {!ArrayBuffer|!ArrayBufferView} buffer
         * @return {!Uint8Array}
         */
        function createUint8ArrayBufferView(buffer)
        {
            return buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        }

        if (!this._cachedBufferData) {
            for (var i = this._calls.length - 1; i >= 0; --i) {
                var call = this._calls[i];
                if (call.functionName() === "bufferData") {
                    var sizeOrData = /** @type {number|!ArrayBuffer|!ArrayBufferView} */ (call.args()[1]);
                    if (typeof sizeOrData === "number")
                        this._cachedBufferData = new ArrayBuffer(sizeOrData);
                    else
                        this._cachedBufferData = sizeOrData;
                    this._lastBufferSubDataIndex = i + 1;
                    break;
                }
            }
            if (!this._cachedBufferData)
                return null;
        }

        // Apply any "bufferSubData" calls that have not been applied yet.
        var bufferDataView;
        while (this._lastBufferSubDataIndex < this._calls.length) {
            var call = this._calls[this._lastBufferSubDataIndex++];
            if (call.functionName() !== "bufferSubData")
                continue;
            var offset = /** @type {number} */ (call.args()[1]);
            var data = /** @type {!ArrayBuffer|!ArrayBufferView} */ (call.args()[2]);
            var view = createUint8ArrayBufferView(data);
            if (!bufferDataView)
                bufferDataView = createUint8ArrayBufferView(this._cachedBufferData);
            bufferDataView.set(view, offset);

            var isFullReplacement = (offset === 0 && bufferDataView.length === view.length);
            if (this._cachedBufferData instanceof ArrayBuffer) {
                // The buffer data has no type yet. Try to guess from the "bufferSubData" call.
                var typedArrayClass = TypeUtils.typedArrayClass(data);
                if (typedArrayClass)
                    this._cachedBufferData = new typedArrayClass(this._cachedBufferData); // Does not copy the buffer.
            } else if (isFullReplacement) {
                var typedArrayClass = TypeUtils.typedArrayClass(data);
                if (typedArrayClass) {
                    var typedArrayData = /** @type {!ArrayBufferView} */ (data);
                    this._cachedBufferData = new typedArrayClass(this._cachedBufferData.buffer, this._cachedBufferData.byteOffset, typedArrayData.length); // Does not copy the buffer.
                }
            }
        }

        if (this._cachedBufferData instanceof ArrayBuffer) {
            // If we failed to guess the data type yet, use Uint8Array.
            return new Uint8Array(this._cachedBufferData);
        }
        return this._cachedBufferData;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();
        var buffer = this.wrappedObject();
        if (!gl || !buffer)
            return result;
        result.push({ name: "isBuffer", value: gl.isBuffer(buffer) });
        result.push({ name: "context", value: this.contextResource() });

        var target = this._state.bindTarget;
        if (typeof target !== "number")
            return result;

        var bindingParameter;
        switch (target) {
        case gl.ARRAY_BUFFER:
            bindingParameter = gl.ARRAY_BUFFER_BINDING;
            break;
        case gl.ELEMENT_ARRAY_BUFFER:
            bindingParameter = gl.ELEMENT_ARRAY_BUFFER_BINDING;
            break;
        default:
            console.error("ASSERT_NOT_REACHED: unknown buffer target " + target);
            return result;
        }
        result.push({ name: "target", value: target, valueIsEnum: true });

        var oldBuffer = /** @type {!WebGLBuffer} */ (gl.getParameter(bindingParameter));
        if (oldBuffer !== buffer)
            gl.bindBuffer(target, buffer);

        var bufferParameters = ["BUFFER_SIZE", "BUFFER_USAGE"];
        glResource.queryStateValues(gl.getBufferParameter, target, bufferParameters, result);

        if (oldBuffer !== buffer)
            gl.bindBuffer(target, oldBuffer);

        try {
            var data = this.cachedBufferData();
            if (data)
                result.push({ name: "bufferData", value: data });
        } catch (e) {
            console.error("Exception while restoring bufferData", e);
        }

        return result;
    },

    /**
     * @param {!Call} call
     */
    pushCall_bufferData: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        delete this._cachedBufferData;
        delete this._lastBufferSubDataIndex;
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_bufferSubData: function(call)
    {
        // FIXME: Optimize memory for bufferSubData.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLFramebufferResource(wrappedObject, name)
{
    WebGLBoundResource.call(this, wrappedObject, name);
}

WebGLFramebufferResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLFramebuffer}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var framebuffer = this.wrappedObject();
        if (!framebuffer)
            return result;
        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();

        var oldFramebuffer = /** @type {!WebGLFramebuffer} */ (gl.getParameter(gl.FRAMEBUFFER_BINDING));
        if (oldFramebuffer !== framebuffer)
            gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);

        var attachmentParameters = ["COLOR_ATTACHMENT0", "DEPTH_ATTACHMENT", "STENCIL_ATTACHMENT"];
        var framebufferParameters = ["FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE", "FRAMEBUFFER_ATTACHMENT_OBJECT_NAME", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL", "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE"];
        for (var i = 0, attachment; attachment = attachmentParameters[i]; ++i) {
            var values = [];
            for (var j = 0, pname; pname = framebufferParameters[j]; ++j) {
                var value = gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl[attachment], gl[pname]);
                value = Resource.forObject(value) || value;
                values.push({ name: pname, value: value, valueIsEnum: WebGLRenderingContextResource.GetResultIsEnum[pname] });
            }
            result.push({ name: attachment, values: values });
        }
        result.push({ name: "isFramebuffer", value: gl.isFramebuffer(framebuffer) });
        result.push({ name: "context", value: this.contextResource() });

        if (oldFramebuffer !== framebuffer)
            gl.bindFramebuffer(gl.FRAMEBUFFER, oldFramebuffer);
        return result;
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {WebGLBoundResource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLRenderbufferResource(wrappedObject, name)
{
    WebGLBoundResource.call(this, wrappedObject, name);
}

WebGLRenderbufferResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLRenderbuffer}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var renderbuffer = this.wrappedObject();
        if (!renderbuffer)
            return result;
        var glResource = WebGLRenderingContextResource.forObject(this);
        var gl = glResource.wrappedObject();

        var oldRenderbuffer = /** @type {!WebGLRenderbuffer} */ (gl.getParameter(gl.RENDERBUFFER_BINDING));
        if (oldRenderbuffer !== renderbuffer)
            gl.bindRenderbuffer(gl.RENDERBUFFER, renderbuffer);

        var renderbufferParameters = ["RENDERBUFFER_WIDTH", "RENDERBUFFER_HEIGHT", "RENDERBUFFER_INTERNAL_FORMAT", "RENDERBUFFER_RED_SIZE", "RENDERBUFFER_GREEN_SIZE", "RENDERBUFFER_BLUE_SIZE", "RENDERBUFFER_ALPHA_SIZE", "RENDERBUFFER_DEPTH_SIZE", "RENDERBUFFER_STENCIL_SIZE"];
        glResource.queryStateValues(gl.getRenderbufferParameter, gl.RENDERBUFFER, renderbufferParameters, result);
        result.push({ name: "isRenderbuffer", value: gl.isRenderbuffer(renderbuffer) });
        result.push({ name: "context", value: this.contextResource() });

        if (oldRenderbuffer !== renderbuffer)
            gl.bindRenderbuffer(gl.RENDERBUFFER, oldRenderbuffer);
        return result;
    },

    /**
     * @override
     * @param {!Call} call
     */
    pushCall: function(call)
    {
        // FIXME: remove any older calls that no longer contribute to the resource state.
        WebGLBoundResource.prototype.pushCall.call(this, call);
    },

    __proto__: WebGLBoundResource.prototype
}

/**
 * @constructor
 * @extends {Resource}
 * @param {!Object} wrappedObject
 * @param {string} name
 */
function WebGLUniformLocationResource(wrappedObject, name)
{
    Resource.call(this, wrappedObject, name);
}

WebGLUniformLocationResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLUniformLocation}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @return {?WebGLProgramResource}
     */
    program: function()
    {
        var call = this._calls[0];
        if (call && call.functionName() === "getUniformLocation")
            return /** @type {!WebGLProgramResource} */ (Resource.forObject(call.args()[0]));
        console.error("ASSERT_NOT_REACHED: Failed to restore WebGLUniformLocation from the log.", call);
        return null;
    },

    /**
     * @return {string}
     */
    name: function()
    {
        var call = this._calls[0];
        if (call && call.functionName() === "getUniformLocation")
            return call.args()[1];
        console.error("ASSERT_NOT_REACHED: Failed to restore WebGLUniformLocation from the log.", call);
        return "";
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var location = this.wrappedObject();
        if (!location)
            return result;
        var programResource = this.program();
        var program = programResource && programResource.wrappedObject();
        if (!program)
            return result;
        var gl = WebGLRenderingContextResource.forObject(this).wrappedObject();
        var uniformValue = gl.getUniform(program, location);
        var name = this.name();
        result.push({ name: "name", value: name });
        result.push({ name: "program", value: programResource });
        result.push({ name: "value", value: uniformValue });
        result.push({ name: "context", value: this.contextResource() });

        if (typeof this._type !== "number") {
            var altName = name + "[0]";
            var uniformsCount = /** @type {number} */ (gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS));
            for (var i = 0; i < uniformsCount; ++i) {
                var activeInfo = gl.getActiveUniform(program, i);
                if (!activeInfo)
                    continue;
                if (activeInfo.name === name || activeInfo.name === altName) {
                    this._type = activeInfo.type;
                    this._size = activeInfo.size;
                    if (activeInfo.name === name)
                        break;
                }
            }
        }
        if (typeof this._type === "number")
            result.push({ name: "type", value: this._type, valueIsEnum: true });
        if (typeof this._size === "number")
            result.push({ name: "size", value: this._size });

        return result;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        data.type = this._type;
        data.size = this._size;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        this._type = data.type;
        this._size = data.size;
        Resource.prototype._doReplayCalls.call(this, data, cache);
    },

    __proto__: Resource.prototype
}

/**
 * @constructor
 * @extends {ContextResource}
 * @param {!WebGLRenderingContext} glContext
 */
function WebGLRenderingContextResource(glContext)
{
    ContextResource.call(this, glContext, "WebGLRenderingContext");
    /** @type {?Object.<number, boolean>} */
    this._customErrors = null;
    /** @type {!Object.<string, string>} */
    this._extensions = {};
    /** @type {!Object.<string, number>} */
    this._extensionEnums = {};
}

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.GLCapabilities = [
    "BLEND",
    "CULL_FACE",
    "DEPTH_TEST",
    "DITHER",
    "POLYGON_OFFSET_FILL",
    "SAMPLE_ALPHA_TO_COVERAGE",
    "SAMPLE_COVERAGE",
    "SCISSOR_TEST",
    "STENCIL_TEST"
];

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.PixelStoreParameters = [
    "PACK_ALIGNMENT",
    "UNPACK_ALIGNMENT",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    "UNPACK_FLIP_Y_WEBGL",
    "UNPACK_PREMULTIPLY_ALPHA_WEBGL"
];

/**
 * @const
 * @type {!Array.<string>}
 */
WebGLRenderingContextResource.StateParameters = [
    "ACTIVE_TEXTURE",
    "ARRAY_BUFFER_BINDING",
    "BLEND_COLOR",
    "BLEND_DST_ALPHA",
    "BLEND_DST_RGB",
    "BLEND_EQUATION_ALPHA",
    "BLEND_EQUATION_RGB",
    "BLEND_SRC_ALPHA",
    "BLEND_SRC_RGB",
    "COLOR_CLEAR_VALUE",
    "COLOR_WRITEMASK",
    "CULL_FACE_MODE",
    "CURRENT_PROGRAM",
    "DEPTH_CLEAR_VALUE",
    "DEPTH_FUNC",
    "DEPTH_RANGE",
    "DEPTH_WRITEMASK",
    "ELEMENT_ARRAY_BUFFER_BINDING",
    "FRAGMENT_SHADER_DERIVATIVE_HINT_OES", // OES_standard_derivatives extension
    "FRAMEBUFFER_BINDING",
    "FRONT_FACE",
    "GENERATE_MIPMAP_HINT",
    "LINE_WIDTH",
    "PACK_ALIGNMENT",
    "POLYGON_OFFSET_FACTOR",
    "POLYGON_OFFSET_UNITS",
    "RENDERBUFFER_BINDING",
    "SAMPLE_COVERAGE_INVERT",
    "SAMPLE_COVERAGE_VALUE",
    "SCISSOR_BOX",
    "STENCIL_BACK_FAIL",
    "STENCIL_BACK_FUNC",
    "STENCIL_BACK_PASS_DEPTH_FAIL",
    "STENCIL_BACK_PASS_DEPTH_PASS",
    "STENCIL_BACK_REF",
    "STENCIL_BACK_VALUE_MASK",
    "STENCIL_BACK_WRITEMASK",
    "STENCIL_CLEAR_VALUE",
    "STENCIL_FAIL",
    "STENCIL_FUNC",
    "STENCIL_PASS_DEPTH_FAIL",
    "STENCIL_PASS_DEPTH_PASS",
    "STENCIL_REF",
    "STENCIL_VALUE_MASK",
    "STENCIL_WRITEMASK",
    "UNPACK_ALIGNMENT",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    "UNPACK_FLIP_Y_WEBGL",
    "UNPACK_PREMULTIPLY_ALPHA_WEBGL",
    "VERTEX_ARRAY_BINDING_OES", // OES_vertex_array_object extension
    "VIEWPORT"
];

/**
 * True for those enums that return also an enum via a getter API method (e.g. getParameter, getShaderParameter, etc.).
 * @const
 * @type {!Object.<string, boolean>}
 */
WebGLRenderingContextResource.GetResultIsEnum = TypeUtils.createPrefixedPropertyNamesSet([
    // gl.getParameter()
    "ACTIVE_TEXTURE",
    "BLEND_DST_ALPHA",
    "BLEND_DST_RGB",
    "BLEND_EQUATION_ALPHA",
    "BLEND_EQUATION_RGB",
    "BLEND_SRC_ALPHA",
    "BLEND_SRC_RGB",
    "CULL_FACE_MODE",
    "DEPTH_FUNC",
    "FRONT_FACE",
    "GENERATE_MIPMAP_HINT",
    "FRAGMENT_SHADER_DERIVATIVE_HINT_OES",
    "STENCIL_BACK_FAIL",
    "STENCIL_BACK_FUNC",
    "STENCIL_BACK_PASS_DEPTH_FAIL",
    "STENCIL_BACK_PASS_DEPTH_PASS",
    "STENCIL_FAIL",
    "STENCIL_FUNC",
    "STENCIL_PASS_DEPTH_FAIL",
    "STENCIL_PASS_DEPTH_PASS",
    "UNPACK_COLORSPACE_CONVERSION_WEBGL",
    // gl.getBufferParameter()
    "BUFFER_USAGE",
    // gl.getFramebufferAttachmentParameter()
    "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE",
    // gl.getRenderbufferParameter()
    "RENDERBUFFER_INTERNAL_FORMAT",
    // gl.getTexParameter()
    "TEXTURE_MAG_FILTER",
    "TEXTURE_MIN_FILTER",
    "TEXTURE_WRAP_S",
    "TEXTURE_WRAP_T",
    // gl.getShaderParameter()
    "SHADER_TYPE",
    // gl.getVertexAttrib()
    "VERTEX_ATTRIB_ARRAY_TYPE"
]);

/**
 * @const
 * @type {!Object.<string, boolean>}
 */
WebGLRenderingContextResource.DrawingMethods = TypeUtils.createPrefixedPropertyNamesSet([
    "clear",
    "drawArrays",
    "drawElements"
]);

/**
 * @param {*} obj
 * @return {?WebGLRenderingContextResource}
 */
WebGLRenderingContextResource.forObject = function(obj)
{
    var resource = Resource.forObject(obj);
    if (!resource)
        return null;
    resource = resource.contextResource();
    return (resource instanceof WebGLRenderingContextResource) ? resource : null;
}

WebGLRenderingContextResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!WebGLRenderingContext}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {string}
     */
    toDataURL: function()
    {
        return this.wrappedObject().canvas.toDataURL();
    },

    /**
     * @return {!Array.<number>}
     */
    getAllErrors: function()
    {
        var errors = [];
        var gl = this.wrappedObject();
        if (gl) {
            while (true) {
                var error = gl.getError();
                if (error === gl.NO_ERROR)
                    break;
                this.clearError(error);
                errors.push(error);
            }
        }
        if (this._customErrors) {
            for (var key in this._customErrors) {
                var error = Number(key);
                errors.push(error);
            }
            delete this._customErrors;
        }
        return errors;
    },

    /**
     * @param {!Array.<number>} errors
     */
    restoreErrors: function(errors)
    {
        var gl = this.wrappedObject();
        if (gl) {
            var wasError = false;
            while (gl.getError() !== gl.NO_ERROR)
                wasError = true;
            console.assert(!wasError, "Error(s) while capturing current WebGL state.");
        }
        if (!errors.length)
            delete this._customErrors;
        else {
            this._customErrors = {};
            for (var i = 0, n = errors.length; i < n; ++i)
                this._customErrors[errors[i]] = true;
        }
    },

    /**
     * @param {number} error
     */
    clearError: function(error)
    {
        if (this._customErrors)
            delete this._customErrors[error];
    },

    /**
     * @return {number}
     */
    nextError: function()
    {
        if (this._customErrors) {
            for (var key in this._customErrors) {
                var error = Number(key);
                delete this._customErrors[error];
                return error;
            }
        }
        delete this._customErrors;
        var gl = this.wrappedObject();
        return gl ? gl.NO_ERROR : 0;
    },

    /**
     * @param {string} name
     * @param {?Object} obj
     */
    registerWebGLExtension: function(name, obj)
    {
        // FIXME: Wrap OES_vertex_array_object extension.
        var lowerName = name.toLowerCase();
        if (obj && !this._extensions[lowerName]) {
            this._extensions[lowerName] = name;
            for (var property in obj) {
                if (TypeUtils.isEnumPropertyName(property, obj))
                    this._extensionEnums[property] = /** @type {number} */ (obj[property]);
            }
        }
    },

    /**
     * @param {string} name
     * @return {number|undefined}
     */
    _enumValueForName: function(name)
    {
        if (typeof this._extensionEnums[name] === "number")
            return this._extensionEnums[name];
        var gl = this.wrappedObject();
        return (typeof gl[name] === "number" ? gl[name] : undefined);
    },

    /**
     * @param {function(this:WebGLRenderingContext, T, number):*} func
     * @param {T} targetOrWebGLObject
     * @param {!Array.<string>} pnames
     * @param {!Array.<!TypeUtils.InternalResourceStateDescriptor>} output
     * @template T
     */
    queryStateValues: function(func, targetOrWebGLObject, pnames, output)
    {
        var gl = this.wrappedObject();
        for (var i = 0, pname; pname = pnames[i]; ++i) {
            var enumValue = this._enumValueForName(pname);
            if (typeof enumValue !== "number")
                continue;
            var value = func.call(gl, targetOrWebGLObject, enumValue);
            value = Resource.forObject(value) || value;
            output.push({ name: pname, value: value, valueIsEnum: WebGLRenderingContextResource.GetResultIsEnum[pname] });
        }
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        /**
         * @param {!Object} obj
         * @param {!Array.<!TypeUtils.InternalResourceStateDescriptor>} output
         */
        function convertToStateDescriptors(obj, output)
        {
            for (var pname in obj)
                output.push({ name: pname, value: obj[pname], valueIsEnum: WebGLRenderingContextResource.GetResultIsEnum[pname] });
        }

        var gl = this.wrappedObject();
        var glState = this._internalCurrentState(null);

        // VERTEX_ATTRIB_ARRAYS
        var vertexAttribStates = [];
        for (var i = 0, n = glState.VERTEX_ATTRIB_ARRAYS.length; i < n; ++i) {
            var pname = "" + i;
            var values = [];
            convertToStateDescriptors(glState.VERTEX_ATTRIB_ARRAYS[i], values);
            vertexAttribStates.push({ name: pname, values: values });
        }
        delete glState.VERTEX_ATTRIB_ARRAYS;

        // TEXTURE_UNITS
        var textureUnits = [];
        for (var i = 0, n = glState.TEXTURE_UNITS.length; i < n; ++i) {
            var pname = "TEXTURE" + i;
            var values = [];
            convertToStateDescriptors(glState.TEXTURE_UNITS[i], values);
            textureUnits.push({ name: pname, values: values });
        }
        delete glState.TEXTURE_UNITS;

        var result = [];
        convertToStateDescriptors(glState, result);
        result.push({ name: "VERTEX_ATTRIB_ARRAYS", values: vertexAttribStates, isArray: true });
        result.push({ name: "TEXTURE_UNITS", values: textureUnits, isArray: true });

        var textureBindingParameters = ["TEXTURE_BINDING_2D", "TEXTURE_BINDING_CUBE_MAP"];
        for (var i = 0, pname; pname = textureBindingParameters[i]; ++i) {
            var value = gl.getParameter(gl[pname]);
            value = Resource.forObject(value) || value;
            result.push({ name: pname, value: value });
        }

        // ENABLED_EXTENSIONS
        var enabledExtensions = [];
        for (var lowerName in this._extensions) {
            var pname = this._extensions[lowerName];
            var value = gl.getExtension(pname);
            value = Resource.forObject(value) || value;
            enabledExtensions.push({ name: pname, value: value });
        }
        result.push({ name: "ENABLED_EXTENSIONS", values: enabledExtensions, isArray: true });

        return result;
    },

    /**
     * @param {?Cache.<!ReplayableResource>} cache
     * @return {!Object.<string, *>}
     */
    _internalCurrentState: function(cache)
    {
        /**
         * @param {!Resource|*} obj
         * @return {!Resource|!ReplayableResource|*}
         */
        function maybeToReplayable(obj)
        {
            return cache ? Resource.toReplayable(obj, cache) : (Resource.forObject(obj) || obj);
        }

        var gl = this.wrappedObject();
        var originalErrors = this.getAllErrors();

        // Take a full GL state snapshot.
        var glState = Object.create(null);
        WebGLRenderingContextResource.GLCapabilities.forEach(function(parameter) {
            glState[parameter] = gl.isEnabled(gl[parameter]);
        });
        for (var i = 0, pname; pname = WebGLRenderingContextResource.StateParameters[i]; ++i) {
            var enumValue = this._enumValueForName(pname);
            if (typeof enumValue === "number")
                glState[pname] = maybeToReplayable(gl.getParameter(enumValue));
        }

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = /** @type {number} */ (gl.getParameter(gl.MAX_VERTEX_ATTRIBS));
        var vertexAttribParameters = [
            "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING",
            "VERTEX_ATTRIB_ARRAY_ENABLED",
            "VERTEX_ATTRIB_ARRAY_SIZE",
            "VERTEX_ATTRIB_ARRAY_STRIDE",
            "VERTEX_ATTRIB_ARRAY_TYPE",
            "VERTEX_ATTRIB_ARRAY_NORMALIZED",
            "CURRENT_VERTEX_ATTRIB",
            "VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE" // ANGLE_instanced_arrays extension
        ];
        var vertexAttribStates = [];
        for (var index = 0; index < maxVertexAttribs; ++index) {
            var state = Object.create(null);
            for (var i = 0, pname; pname = vertexAttribParameters[i]; ++i) {
                var enumValue = this._enumValueForName(pname);
                if (typeof enumValue === "number")
                    state[pname] = maybeToReplayable(gl.getVertexAttrib(index, enumValue));
            }
            state.VERTEX_ATTRIB_ARRAY_POINTER = gl.getVertexAttribOffset(index, gl.VERTEX_ATTRIB_ARRAY_POINTER);
            vertexAttribStates.push(state);
        }
        glState.VERTEX_ATTRIB_ARRAYS = vertexAttribStates;

        // TEXTURE_UNITS
        var savedActiveTexture = /** @type {number} */ (gl.getParameter(gl.ACTIVE_TEXTURE));
        var maxTextureImageUnits = /** @type {number} */ (gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS));
        var textureUnits = [];
        for (var i = 0; i < maxTextureImageUnits; ++i) {
            gl.activeTexture(gl.TEXTURE0 + i);
            var state = Object.create(null);
            state.TEXTURE_2D = maybeToReplayable(gl.getParameter(gl.TEXTURE_BINDING_2D));
            state.TEXTURE_CUBE_MAP = maybeToReplayable(gl.getParameter(gl.TEXTURE_BINDING_CUBE_MAP));
            textureUnits.push(state);
        }
        glState.TEXTURE_UNITS = textureUnits;
        gl.activeTexture(savedActiveTexture);

        this.restoreErrors(originalErrors);
        return glState;
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var gl = this.wrappedObject();
        data.originalCanvas = gl.canvas;
        data.originalContextAttributes = gl.getContextAttributes();
        data.extensions = TypeUtils.cloneObject(this._extensions);
        data.extensionEnums = TypeUtils.cloneObject(this._extensionEnums);
        data.glState = this._internalCurrentState(cache);
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        this._customErrors = null;
        this._extensions = TypeUtils.cloneObject(data.extensions) || {};
        this._extensionEnums = TypeUtils.cloneObject(data.extensionEnums) || {};

        var canvas = data.originalCanvas.cloneNode(true);
        var replayContext = null;
        var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
        for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
            replayContext = canvas.getContext(contextId, data.originalContextAttributes);
            if (replayContext)
                break;
        }

        console.assert(replayContext, "Failed to create a WebGLRenderingContext for the replay.");

        var gl = /** @type {!WebGLRenderingContext} */ (Resource.wrappedObject(replayContext));
        this.setWrappedObject(gl);

        // Enable corresponding WebGL extensions.
        for (var name in this._extensions)
            gl.getExtension(name);

        var glState = data.glState;
        gl.bindFramebuffer(gl.FRAMEBUFFER, /** @type {!WebGLFramebuffer} */ (ReplayableResource.replay(glState.FRAMEBUFFER_BINDING, cache)));
        gl.bindRenderbuffer(gl.RENDERBUFFER, /** @type {!WebGLRenderbuffer} */ (ReplayableResource.replay(glState.RENDERBUFFER_BINDING, cache)));

        // Enable or disable server-side GL capabilities.
        WebGLRenderingContextResource.GLCapabilities.forEach(function(parameter) {
            console.assert(parameter in glState);
            if (glState[parameter])
                gl.enable(gl[parameter]);
            else
                gl.disable(gl[parameter]);
        });

        gl.blendColor(glState.BLEND_COLOR[0], glState.BLEND_COLOR[1], glState.BLEND_COLOR[2], glState.BLEND_COLOR[3]);
        gl.blendEquationSeparate(glState.BLEND_EQUATION_RGB, glState.BLEND_EQUATION_ALPHA);
        gl.blendFuncSeparate(glState.BLEND_SRC_RGB, glState.BLEND_DST_RGB, glState.BLEND_SRC_ALPHA, glState.BLEND_DST_ALPHA);
        gl.clearColor(glState.COLOR_CLEAR_VALUE[0], glState.COLOR_CLEAR_VALUE[1], glState.COLOR_CLEAR_VALUE[2], glState.COLOR_CLEAR_VALUE[3]);
        gl.clearDepth(glState.DEPTH_CLEAR_VALUE);
        gl.clearStencil(glState.STENCIL_CLEAR_VALUE);
        gl.colorMask(glState.COLOR_WRITEMASK[0], glState.COLOR_WRITEMASK[1], glState.COLOR_WRITEMASK[2], glState.COLOR_WRITEMASK[3]);
        gl.cullFace(glState.CULL_FACE_MODE);
        gl.depthFunc(glState.DEPTH_FUNC);
        gl.depthMask(glState.DEPTH_WRITEMASK);
        gl.depthRange(glState.DEPTH_RANGE[0], glState.DEPTH_RANGE[1]);
        gl.frontFace(glState.FRONT_FACE);
        gl.hint(gl.GENERATE_MIPMAP_HINT, glState.GENERATE_MIPMAP_HINT);
        gl.lineWidth(glState.LINE_WIDTH);

        var enumValue = this._enumValueForName("FRAGMENT_SHADER_DERIVATIVE_HINT_OES");
        if (typeof enumValue === "number")
            gl.hint(enumValue, glState.FRAGMENT_SHADER_DERIVATIVE_HINT_OES);

        WebGLRenderingContextResource.PixelStoreParameters.forEach(function(parameter) {
            gl.pixelStorei(gl[parameter], glState[parameter]);
        });

        gl.polygonOffset(glState.POLYGON_OFFSET_FACTOR, glState.POLYGON_OFFSET_UNITS);
        gl.sampleCoverage(glState.SAMPLE_COVERAGE_VALUE, glState.SAMPLE_COVERAGE_INVERT);
        gl.stencilFuncSeparate(gl.FRONT, glState.STENCIL_FUNC, glState.STENCIL_REF, glState.STENCIL_VALUE_MASK);
        gl.stencilFuncSeparate(gl.BACK, glState.STENCIL_BACK_FUNC, glState.STENCIL_BACK_REF, glState.STENCIL_BACK_VALUE_MASK);
        gl.stencilOpSeparate(gl.FRONT, glState.STENCIL_FAIL, glState.STENCIL_PASS_DEPTH_FAIL, glState.STENCIL_PASS_DEPTH_PASS);
        gl.stencilOpSeparate(gl.BACK, glState.STENCIL_BACK_FAIL, glState.STENCIL_BACK_PASS_DEPTH_FAIL, glState.STENCIL_BACK_PASS_DEPTH_PASS);
        gl.stencilMaskSeparate(gl.FRONT, glState.STENCIL_WRITEMASK);
        gl.stencilMaskSeparate(gl.BACK, glState.STENCIL_BACK_WRITEMASK);

        gl.scissor(glState.SCISSOR_BOX[0], glState.SCISSOR_BOX[1], glState.SCISSOR_BOX[2], glState.SCISSOR_BOX[3]);
        gl.viewport(glState.VIEWPORT[0], glState.VIEWPORT[1], glState.VIEWPORT[2], glState.VIEWPORT[3]);

        gl.useProgram(/** @type {!WebGLProgram} */ (ReplayableResource.replay(glState.CURRENT_PROGRAM, cache)));

        // VERTEX_ATTRIB_ARRAYS
        var maxVertexAttribs = /** @type {number} */ (gl.getParameter(gl.MAX_VERTEX_ATTRIBS));
        for (var i = 0; i < maxVertexAttribs; ++i) {
            var state = glState.VERTEX_ATTRIB_ARRAYS[i] || {};
            if (state.VERTEX_ATTRIB_ARRAY_ENABLED)
                gl.enableVertexAttribArray(i);
            else
                gl.disableVertexAttribArray(i);
            if (state.CURRENT_VERTEX_ATTRIB)
                gl.vertexAttrib4fv(i, state.CURRENT_VERTEX_ATTRIB);
            var buffer = /** @type {!WebGLBuffer} */ (ReplayableResource.replay(state.VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, cache));
            if (buffer) {
                gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
                gl.vertexAttribPointer(i, state.VERTEX_ATTRIB_ARRAY_SIZE, state.VERTEX_ATTRIB_ARRAY_TYPE, state.VERTEX_ATTRIB_ARRAY_NORMALIZED, state.VERTEX_ATTRIB_ARRAY_STRIDE, state.VERTEX_ATTRIB_ARRAY_POINTER);
            }
        }
        gl.bindBuffer(gl.ARRAY_BUFFER, /** @type {!WebGLBuffer} */ (ReplayableResource.replay(glState.ARRAY_BUFFER_BINDING, cache)));
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, /** @type {!WebGLBuffer} */ (ReplayableResource.replay(glState.ELEMENT_ARRAY_BUFFER_BINDING, cache)));

        // TEXTURE_UNITS
        var maxTextureImageUnits = /** @type {number} */ (gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS));
        for (var i = 0; i < maxTextureImageUnits; ++i) {
            gl.activeTexture(gl.TEXTURE0 + i);
            var state = glState.TEXTURE_UNITS[i] || {};
            gl.bindTexture(gl.TEXTURE_2D, /** @type {!WebGLTexture} */ (ReplayableResource.replay(state.TEXTURE_2D, cache)));
            gl.bindTexture(gl.TEXTURE_CUBE_MAP, /** @type {!WebGLTexture} */ (ReplayableResource.replay(state.TEXTURE_CUBE_MAP, cache)));
        }
        gl.activeTexture(glState.ACTIVE_TEXTURE);

        ContextResource.prototype._doReplayCalls.call(this, data, cache);
    },

    /**
     * @param {!Object|number} target
     * @return {?Resource}
     */
    currentBinding: function(target)
    {
        var resource = Resource.forObject(target);
        if (resource)
            return resource;
        var gl = this.wrappedObject();
        var bindingParameter;
        var bindMethodName;
        target = +target; // Explicitly convert to a number.
        var bindMethodTarget = target;
        switch (target) {
        case gl.ARRAY_BUFFER:
            bindingParameter = gl.ARRAY_BUFFER_BINDING;
            bindMethodName = "bindBuffer";
            break;
        case gl.ELEMENT_ARRAY_BUFFER:
            bindingParameter = gl.ELEMENT_ARRAY_BUFFER_BINDING;
            bindMethodName = "bindBuffer";
            break;
        case gl.TEXTURE_2D:
            bindingParameter = gl.TEXTURE_BINDING_2D;
            bindMethodName = "bindTexture";
            break;
        case gl.TEXTURE_CUBE_MAP:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_X:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_X:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_Y:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case gl.TEXTURE_CUBE_MAP_POSITIVE_Z:
        case gl.TEXTURE_CUBE_MAP_NEGATIVE_Z:
            bindingParameter = gl.TEXTURE_BINDING_CUBE_MAP;
            bindMethodTarget = gl.TEXTURE_CUBE_MAP;
            bindMethodName = "bindTexture";
            break;
        case gl.FRAMEBUFFER:
            bindingParameter = gl.FRAMEBUFFER_BINDING;
            bindMethodName = "bindFramebuffer";
            break;
        case gl.RENDERBUFFER:
            bindingParameter = gl.RENDERBUFFER_BINDING;
            bindMethodName = "bindRenderbuffer";
            break;
        default:
            console.error("ASSERT_NOT_REACHED: unknown binding target " + target);
            return null;
        }
        resource = Resource.forObject(gl.getParameter(bindingParameter));
        if (resource)
            resource.pushBinding(bindMethodTarget, bindMethodName);
        return resource;
    },

    /**
     * @override
     * @param {!Call} call
     */
    onCallReplayed: function(call)
    {
        var functionName = call.functionName();
        var args = call.args();
        switch (functionName) {
        case "bindBuffer":
        case "bindFramebuffer":
        case "bindRenderbuffer":
        case "bindTexture":
            // Update BINDING state for Resources in the replay world.
            var resource = Resource.forObject(args[1]);
            if (resource)
                resource.pushBinding(args[0], functionName);
            break;
        case "getExtension":
            this.registerWebGLExtension(args[0], /** @type {!Object} */ (call.result()));
            break;
        case "bufferData":
            var resource = /** @type {!WebGLBufferResource} */ (this.currentBinding(args[0]));
            if (resource)
                resource.pushCall_bufferData(call);
            break;
        case "bufferSubData":
            var resource = /** @type {!WebGLBufferResource} */ (this.currentBinding(args[0]));
            if (resource)
                resource.pushCall_bufferSubData(call);
            break;
        }
    },

    /**
     * @override
     * @return {!Object.<string, !Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = WebGLRenderingContextResource._wrapFunctions;
        if (!wrapFunctions) {
            wrapFunctions = Object.create(null);

            wrapFunctions["createBuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLBufferResource, "WebGLBuffer");
            wrapFunctions["createShader"] = Resource.WrapFunction.resourceFactoryMethod(WebGLShaderResource, "WebGLShader");
            wrapFunctions["createProgram"] = Resource.WrapFunction.resourceFactoryMethod(WebGLProgramResource, "WebGLProgram");
            wrapFunctions["createTexture"] = Resource.WrapFunction.resourceFactoryMethod(WebGLTextureResource, "WebGLTexture");
            wrapFunctions["createFramebuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLFramebufferResource, "WebGLFramebuffer");
            wrapFunctions["createRenderbuffer"] = Resource.WrapFunction.resourceFactoryMethod(WebGLRenderbufferResource, "WebGLRenderbuffer");
            wrapFunctions["getUniformLocation"] = Resource.WrapFunction.resourceFactoryMethod(WebGLUniformLocationResource, "WebGLUniformLocation");

            stateModifyingWrapFunction("bindAttribLocation");
            stateModifyingWrapFunction("compileShader");
            stateModifyingWrapFunction("detachShader");
            stateModifyingWrapFunction("linkProgram");
            stateModifyingWrapFunction("shaderSource");
            stateModifyingWrapFunction("bufferData", WebGLBufferResource.prototype.pushCall_bufferData);
            stateModifyingWrapFunction("bufferSubData", WebGLBufferResource.prototype.pushCall_bufferSubData);
            stateModifyingWrapFunction("compressedTexImage2D");
            stateModifyingWrapFunction("compressedTexSubImage2D");
            stateModifyingWrapFunction("copyTexImage2D", WebGLTextureResource.prototype.pushCall_copyTexImage2D);
            stateModifyingWrapFunction("copyTexSubImage2D", WebGLTextureResource.prototype.pushCall_copyTexImage2D);
            stateModifyingWrapFunction("generateMipmap");
            stateModifyingWrapFunction("texImage2D");
            stateModifyingWrapFunction("texSubImage2D");
            stateModifyingWrapFunction("texParameterf", WebGLTextureResource.prototype.pushCall_texParameter);
            stateModifyingWrapFunction("texParameteri", WebGLTextureResource.prototype.pushCall_texParameter);
            stateModifyingWrapFunction("renderbufferStorage");

            /** @this {Resource.WrapFunction} */
            wrapFunctions["getError"] = function()
            {
                var gl = /** @type {!WebGLRenderingContext} */ (this._originalObject);
                var error = this.result();
                if (error !== gl.NO_ERROR)
                    this._resource.clearError(error);
                else {
                    error = this._resource.nextError();
                    if (error !== gl.NO_ERROR)
                        this.overrideResult(error);
                }
            }

            /**
             * @param {string} name
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["getExtension"] = function(name)
            {
                this._resource.registerWebGLExtension(name, this.result());
            }

            //
            // Register bound WebGL resources.
            //

            /**
             * @param {!WebGLProgram} program
             * @param {!WebGLShader} shader
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["attachShader"] = function(program, shader)
            {
                var resource = this._resource.currentBinding(program);
                if (resource) {
                    resource.pushCall(this.call());
                    var shaderResource = /** @type {!WebGLShaderResource} */ (Resource.forObject(shader));
                    if (shaderResource) {
                        var shaderType = shaderResource.type();
                        resource._registerBoundResource("__attachShader_" + shaderType, shaderResource);
                    }
                }
            }
            /**
             * @param {number} target
             * @param {number} attachment
             * @param {number} objectTarget
             * @param {!WebGLRenderbuffer|!WebGLTexture} obj
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["framebufferRenderbuffer"] = wrapFunctions["framebufferTexture2D"] = function(target, attachment, objectTarget, obj)
            {
                var resource = this._resource.currentBinding(target);
                if (resource) {
                    resource.pushCall(this.call());
                    resource._registerBoundResource("__framebufferAttachmentObjectName", obj);
                }
            }
            /**
             * @param {number} target
             * @param {!Object} obj
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["bindBuffer"] = wrapFunctions["bindFramebuffer"] = wrapFunctions["bindRenderbuffer"] = function(target, obj)
            {
                this._resource.currentBinding(target); // To call WebGLBoundResource.prototype.pushBinding().
                this._resource._registerBoundResource("__bindBuffer_" + target, obj);
            }
            /**
             * @param {number} target
             * @param {!WebGLTexture} obj
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["bindTexture"] = function(target, obj)
            {
                this._resource.currentBinding(target); // To call WebGLBoundResource.prototype.pushBinding().
                var gl = /** @type {!WebGLRenderingContext} */ (this._originalObject);
                var currentTextureBinding = /** @type {number} */ (gl.getParameter(gl.ACTIVE_TEXTURE));
                this._resource._registerBoundResource("__bindTexture_" + target + "_" + currentTextureBinding, obj);
            }
            /**
             * @param {!WebGLProgram} program
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["useProgram"] = function(program)
            {
                this._resource._registerBoundResource("__useProgram", program);
            }
            /**
             * @param {number} index
             * @this {Resource.WrapFunction}
             */
            wrapFunctions["vertexAttribPointer"] = function(index)
            {
                var gl = /** @type {!WebGLRenderingContext} */ (this._originalObject);
                this._resource._registerBoundResource("__vertexAttribPointer_" + index, gl.getParameter(gl.ARRAY_BUFFER_BINDING));
            }

            WebGLRenderingContextResource._wrapFunctions = wrapFunctions;
        }

        /**
         * @param {string} methodName
         * @param {function(this:Resource, !Call)=} pushCallFunc
         */
        function stateModifyingWrapFunction(methodName, pushCallFunc)
        {
            if (pushCallFunc) {
                /**
                 * @param {!Object|number} target
                 * @this {Resource.WrapFunction}
                 */
                wrapFunctions[methodName] = function(target)
                {
                    var resource = this._resource.currentBinding(target);
                    if (resource)
                        pushCallFunc.call(resource, this.call());
                }
            } else {
                /**
                 * @param {!Object|number} target
                 * @this {Resource.WrapFunction}
                 */
                wrapFunctions[methodName] = function(target)
                {
                    var resource = this._resource.currentBinding(target);
                    if (resource)
                        resource.pushCall(this.call());
                }
            }
        }

        return wrapFunctions;
    },

    __proto__: ContextResource.prototype
}

////////////////////////////////////////////////////////////////////////////////
// 2D Canvas
////////////////////////////////////////////////////////////////////////////////

/**
 * @constructor
 * @extends {ContextResource}
 * @param {!CanvasRenderingContext2D} context
 */
function CanvasRenderingContext2DResource(context)
{
    ContextResource.call(this, context, "CanvasRenderingContext2D");
}

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.AttributeProperties = [
    "strokeStyle",
    "fillStyle",
    "globalAlpha",
    "lineWidth",
    "lineCap",
    "lineJoin",
    "miterLimit",
    "shadowOffsetX",
    "shadowOffsetY",
    "shadowBlur",
    "shadowColor",
    "globalCompositeOperation",
    "font",
    "textAlign",
    "textBaseline",
    "lineDashOffset",
    "imageSmoothingEnabled",
    "webkitLineDash",
    "webkitLineDashOffset"
];

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.PathMethods = [
    "beginPath",
    "moveTo",
    "closePath",
    "lineTo",
    "quadraticCurveTo",
    "bezierCurveTo",
    "arcTo",
    "arc",
    "rect"
];

/**
 * @const
 * @type {!Array.<string>}
 */
CanvasRenderingContext2DResource.TransformationMatrixMethods = [
    "scale",
    "rotate",
    "translate",
    "transform",
    "setTransform"
];

/**
 * @const
 * @type {!Object.<string, boolean>}
 */
CanvasRenderingContext2DResource.DrawingMethods = TypeUtils.createPrefixedPropertyNamesSet([
    "clearRect",
    "drawImage",
    "drawImageFromRect",
    "drawCustomFocusRing",
    "drawFocusIfNeeded",
    "fill",
    "fillRect",
    "fillText",
    "putImageData",
    "putImageDataHD",
    "stroke",
    "strokeRect",
    "strokeText"
]);

CanvasRenderingContext2DResource.prototype = {
    /**
     * @override (overrides @return type)
     * @return {!CanvasRenderingContext2D}
     */
    wrappedObject: function()
    {
        return this._wrappedObject;
    },

    /**
     * @override
     * @return {string}
     */
    toDataURL: function()
    {
        return this.wrappedObject().canvas.toDataURL();
    },

    /**
     * @override
     * @return {!Array.<!TypeUtils.InternalResourceStateDescriptor>}
     */
    currentState: function()
    {
        var result = [];
        var state = this._internalCurrentState(null);
        for (var pname in state)
            result.push({ name: pname, value: state[pname] });
        result.push({ name: "context", value: this.contextResource() });
        return result;
    },

    /**
     * @param {?Cache.<!ReplayableResource>} cache
     * @return {!Object.<string, *>}
     */
    _internalCurrentState: function(cache)
    {
        /**
         * @param {!Resource|*} obj
         * @return {!Resource|!ReplayableResource|*}
         */
        function maybeToReplayable(obj)
        {
            return cache ? Resource.toReplayable(obj, cache) : (Resource.forObject(obj) || obj);
        }

        var ctx = this.wrappedObject();
        var state = Object.create(null);
        CanvasRenderingContext2DResource.AttributeProperties.forEach(function(attribute) {
            if (attribute in ctx)
                state[attribute] = maybeToReplayable(ctx[attribute]);
        });
        if (ctx.getLineDash)
            state.lineDash = ctx.getLineDash();
        return state;
    },

    /**
     * @param {?Object.<string, *>} state
     * @param {!Cache.<!Resource>} cache
     */
    _applyAttributesState: function(state, cache)
    {
        if (!state)
            return;
        var ctx = this.wrappedObject();
        for (var attribute in state) {
            if (attribute === "lineDash") {
                if (ctx.setLineDash)
                    ctx.setLineDash(/** @type {!Array.<number>} */ (state[attribute]));
            } else
                ctx[attribute] = ReplayableResource.replay(state[attribute], cache);
        }
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!ReplayableResource>} cache
     */
    _populateReplayableData: function(data, cache)
    {
        var ctx = this.wrappedObject();
        // FIXME: Convert resources in the state (CanvasGradient, CanvasPattern) to Replayable.
        data.currentAttributes = this._internalCurrentState(null);
        data.originalCanvasCloned = TypeUtils.cloneIntoCanvas(/** @type {!HTMLCanvasElement} */ (ctx.canvas));
        if (ctx.getContextAttributes)
            data.originalContextAttributes = ctx.getContextAttributes();
    },

    /**
     * @override
     * @param {!Object} data
     * @param {!Cache.<!Resource>} cache
     */
    _doReplayCalls: function(data, cache)
    {
        var canvas = TypeUtils.cloneIntoCanvas(data.originalCanvasCloned);
        var ctx = /** @type {!CanvasRenderingContext2D} */ (Resource.wrappedObject(canvas.getContext("2d", data.originalContextAttributes)));
        this.setWrappedObject(ctx);

        for (var i = 0, n = data.calls.length; i < n; ++i) {
            var replayableCall = /** @type {!ReplayableCall} */ (data.calls[i]);
            if (replayableCall.functionName() === "save")
                this._applyAttributesState(replayableCall.attachment("canvas2dAttributesState"), cache);
            this._calls.push(replayableCall.replay(cache));
        }
        this._applyAttributesState(data.currentAttributes, cache);
    },

    /**
     * @param {!Call} call
     */
    pushCall_setTransform: function(call)
    {
        var saveCallIndex = this._lastIndexOfMatchingSaveCall();
        var index = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods);
        index = Math.max(index, saveCallIndex);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1))
            this._removeAllObsoleteCallsFromLog();
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_beginPath: function(call)
    {
        var index = this._lastIndexOfAnyCall(["clip"]);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.PathMethods, index + 1))
            this._removeAllObsoleteCallsFromLog();
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_save: function(call)
    {
        // FIXME: Convert resources in the state (CanvasGradient, CanvasPattern) to Replayable.
        call.setAttachment("canvas2dAttributesState", this._internalCurrentState(null));
        this.pushCall(call);
    },

    /**
     * @param {!Call} call
     */
    pushCall_restore: function(call)
    {
        var lastIndexOfSave = this._lastIndexOfMatchingSaveCall();
        if (lastIndexOfSave === -1)
            return;
        this._calls[lastIndexOfSave].setAttachment("canvas2dAttributesState", null); // No longer needed, free memory.

        var modified = false;
        if (this._removeCallsFromLog(["clip"], lastIndexOfSave + 1))
            modified = true;

        var lastIndexOfAnyPathMethod = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods);
        var index = Math.max(lastIndexOfSave, lastIndexOfAnyPathMethod);
        if (this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1))
            modified = true;

        if (modified)
            this._removeAllObsoleteCallsFromLog();

        var lastCall = this._calls[this._calls.length - 1];
        if (lastCall && lastCall.functionName() === "save")
            this._calls.pop();
        else
            this.pushCall(call);
    },

    /**
     * @param {number=} fromIndex
     * @return {number}
     */
    _lastIndexOfMatchingSaveCall: function(fromIndex)
    {
        if (typeof fromIndex !== "number")
            fromIndex = this._calls.length - 1;
        else
            fromIndex = Math.min(fromIndex, this._calls.length - 1);
        var stackDepth = 1;
        for (var i = fromIndex; i >= 0; --i) {
            var functionName = this._calls[i].functionName();
            if (functionName === "restore")
                ++stackDepth;
            else if (functionName === "save") {
                --stackDepth;
                if (!stackDepth)
                    return i;
            }
        }
        return -1;
    },

    /**
     * @param {!Array.<string>} functionNames
     * @param {number=} fromIndex
     * @return {number}
     */
    _lastIndexOfAnyCall: function(functionNames, fromIndex)
    {
        if (typeof fromIndex !== "number")
            fromIndex = this._calls.length - 1;
        else
            fromIndex = Math.min(fromIndex, this._calls.length - 1);
        for (var i = fromIndex; i >= 0; --i) {
            if (functionNames.indexOf(this._calls[i].functionName()) !== -1)
                return i;
        }
        return -1;
    },

    _removeAllObsoleteCallsFromLog: function()
    {
        // Remove all PATH methods between clip() and beginPath() calls.
        var lastIndexOfBeginPath = this._lastIndexOfAnyCall(["beginPath"]);
        while (lastIndexOfBeginPath !== -1) {
            var index = this._lastIndexOfAnyCall(["clip"], lastIndexOfBeginPath - 1);
            this._removeCallsFromLog(CanvasRenderingContext2DResource.PathMethods, index + 1, lastIndexOfBeginPath);
            lastIndexOfBeginPath = this._lastIndexOfAnyCall(["beginPath"], index - 1);
        }

        // Remove all TRASFORMATION MATRIX methods before restore() or setTransform() but after any PATH or corresponding save() method.
        var lastRestore = this._lastIndexOfAnyCall(["restore", "setTransform"]);
        while (lastRestore !== -1) {
            var saveCallIndex = this._lastIndexOfMatchingSaveCall(lastRestore - 1);
            var index = this._lastIndexOfAnyCall(CanvasRenderingContext2DResource.PathMethods, lastRestore - 1);
            index = Math.max(index, saveCallIndex);
            this._removeCallsFromLog(CanvasRenderingContext2DResource.TransformationMatrixMethods, index + 1, lastRestore);
            lastRestore = this._lastIndexOfAnyCall(["restore", "setTransform"], index - 1);
        }

        // Remove all save-restore consecutive pairs.
        var restoreCalls = 0;
        for (var i = this._calls.length - 1; i >= 0; --i) {
            var functionName = this._calls[i].functionName();
            if (functionName === "restore") {
                ++restoreCalls;
                continue;
            }
            if (functionName === "save" && restoreCalls > 0) {
                var saveCallIndex = i;
                for (var j = i - 1; j >= 0 && i - j < restoreCalls; --j) {
                    if (this._calls[j].functionName() === "save")
                        saveCallIndex = j;
                    else
                        break;
                }
                this._calls.splice(saveCallIndex, (i - saveCallIndex + 1) * 2);
                i = saveCallIndex;
            }
            restoreCalls = 0;
        }
    },

    /**
     * @param {!Array.<string>} functionNames
     * @param {number} fromIndex
     * @param {number=} toIndex
     * @return {boolean}
     */
    _removeCallsFromLog: function(functionNames, fromIndex, toIndex)
    {
        var oldLength = this._calls.length;
        if (typeof toIndex !== "number")
            toIndex = oldLength;
        else
            toIndex = Math.min(toIndex, oldLength);
        var newIndex = Math.min(fromIndex, oldLength);
        for (var i = newIndex; i < toIndex; ++i) {
            var call = this._calls[i];
            if (functionNames.indexOf(call.functionName()) === -1)
                this._calls[newIndex++] = call;
        }
        if (newIndex >= toIndex)
            return false;
        this._calls.splice(newIndex, toIndex - newIndex);
        return true;
    },

    /**
     * @override
     * @return {!Object.<string, !Function>}
     */
    _customWrapFunctions: function()
    {
        var wrapFunctions = CanvasRenderingContext2DResource._wrapFunctions;
        if (!wrapFunctions) {
            wrapFunctions = Object.create(null);

            wrapFunctions["createLinearGradient"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource, "CanvasGradient");
            wrapFunctions["createRadialGradient"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource, "CanvasGradient");
            wrapFunctions["createPattern"] = Resource.WrapFunction.resourceFactoryMethod(LogEverythingResource, "CanvasPattern");

            for (var i = 0, methodName; methodName = CanvasRenderingContext2DResource.TransformationMatrixMethods[i]; ++i)
                stateModifyingWrapFunction(methodName, methodName === "setTransform" ? this.pushCall_setTransform : undefined);
            for (var i = 0, methodName; methodName = CanvasRenderingContext2DResource.PathMethods[i]; ++i)
                stateModifyingWrapFunction(methodName, methodName === "beginPath" ? this.pushCall_beginPath : undefined);

            stateModifyingWrapFunction("save", this.pushCall_save);
            stateModifyingWrapFunction("restore", this.pushCall_restore);
            stateModifyingWrapFunction("clip");

            CanvasRenderingContext2DResource._wrapFunctions = wrapFunctions;
        }

        /**
         * @param {string} methodName
         * @param {function(this:Resource, !Call)=} func
         */
        function stateModifyingWrapFunction(methodName, func)
        {
            if (func) {
                /** @this {Resource.WrapFunction} */
                wrapFunctions[methodName] = function()
                {
                    func.call(this._resource, this.call());
                }
            } else {
                /** @this {Resource.WrapFunction} */
                wrapFunctions[methodName] = function()
                {
                    this._resource.pushCall(this.call());
                }
            }
        }

        return wrapFunctions;
    },

    __proto__: ContextResource.prototype
}

/**
 * @constructor
 * @param {!Object.<string, boolean>=} drawingMethodNames
 */
function CallFormatter(drawingMethodNames)
{
    this._drawingMethodNames = drawingMethodNames || Object.create(null);
}

CallFormatter.prototype = {
    /**
     * @param {!ReplayableCall} replayableCall
     * @param {string=} objectGroup
     * @return {!Object}
     */
    formatCall: function(replayableCall, objectGroup)
    {
        var result = {};
        var functionName = replayableCall.functionName();
        if (functionName) {
            result.functionName = functionName;
            result.arguments = [];
            var args = replayableCall.args();
            for (var i = 0, n = args.length; i < n; ++i)
                result.arguments.push(this.formatValue(args[i], objectGroup));
            if (replayableCall.result() !== undefined)
                result.result = this.formatValue(replayableCall.result(), objectGroup);
            if (this._drawingMethodNames[functionName])
                result.isDrawingCall = true;
        } else {
            result.property = replayableCall.propertyName();
            result.value = this.formatValue(replayableCall.propertyValue(), objectGroup);
        }
        return result;
    },

    /**
     * @param {*} value
     * @param {string=} objectGroup
     * @return {!CanvasAgent.CallArgument}
     */
    formatValue: function(value, objectGroup)
    {
        if (value instanceof Resource || value instanceof ReplayableResource) {
            return {
                description: value.description(),
                resourceId: CallFormatter.makeStringResourceId(value.id())
            };
        }

        var remoteObject = injectedScript.wrapObject(value, objectGroup || "", true, false);
        var description = remoteObject.description || ("" + value);

        var result = {
            description: description,
            type: /** @type {!CanvasAgent.CallArgumentType} */ (remoteObject.type)
        };
        if (remoteObject.subtype)
            result.subtype = /** @type {!CanvasAgent.CallArgumentSubtype} */ (remoteObject.subtype);
        if (remoteObject.objectId) {
            if (objectGroup)
                result.remoteObject = remoteObject;
            else
                injectedScript.releaseObject(remoteObject.objectId);
        }
        return result;
    },

    /**
     * @param {string} name
     * @return {?string}
     */
    enumValueForName: function(name)
    {
        return null;
    },

    /**
     * @param {number} value
     * @param {!Array.<string>=} options
     * @return {?string}
     */
    enumNameForValue: function(value, options)
    {
        return null;
    },

    /**
     * @param {!Array.<!TypeUtils.InternalResourceStateDescriptor>} descriptors
     * @param {string=} objectGroup
     * @return {!Array.<!CanvasAgent.ResourceStateDescriptor>}
     */
    formatResourceStateDescriptors: function(descriptors, objectGroup)
    {
        var result = [];
        for (var i = 0, n = descriptors.length; i < n; ++i) {
            var d = descriptors[i];
            var item;
            if (d.values)
                item = { name: d.name, values: this.formatResourceStateDescriptors(d.values, objectGroup) };
            else {
                item = { name: d.name, value: this.formatValue(d.value, objectGroup) };
                if (d.valueIsEnum && typeof d.value === "number") {
                    var enumName = this.enumNameForValue(d.value);
                    if (enumName)
                        item.value.enumName = enumName;
                }
            }
            var enumValue = this.enumValueForName(d.name);
            if (enumValue)
                item.enumValueForName = enumValue;
            if (d.isArray)
                item.isArray = true;
            result.push(item);
        }
        return result;
    }
}

/**
 * @const
 * @type {!Object.<string, !CallFormatter>}
 */
CallFormatter._formatters = {};

/**
 * @param {string} resourceName
 * @param {!CallFormatter} callFormatter
 */
CallFormatter.register = function(resourceName, callFormatter)
{
    CallFormatter._formatters[resourceName] = callFormatter;
}

/**
 * @param {!Resource|!ReplayableResource} resource
 * @return {!CallFormatter}
 */
CallFormatter.forResource = function(resource)
{
    var formatter = CallFormatter._formatters[resource.name()];
    if (!formatter) {
        var contextResource = resource.contextResource();
        formatter = (contextResource && CallFormatter._formatters[contextResource.name()]) || new CallFormatter();
    }
    return formatter;
}

/**
 * @param {number} resourceId
 * @return {!CanvasAgent.ResourceId}
 */
CallFormatter.makeStringResourceId = function(resourceId)
{
    return "{\"injectedScriptId\":" + injectedScriptId + ",\"resourceId\":" + resourceId + "}";
}

/**
 * @constructor
 * @extends {CallFormatter}
 * @param {!Object.<string, boolean>} drawingMethodNames
 */
function WebGLCallFormatter(drawingMethodNames)
{
    CallFormatter.call(this, drawingMethodNames);
}

/**
 * NOTE: The code below is generated from the IDL file by the script:
 * /devtools/scripts/check_injected_webgl_calls_info.py
 *
 * @type {!Array.<{aname: string, enum: (!Array.<number>|undefined), bitfield: (!Array.<number>|undefined), returnType: string, hints: (!Array.<string>|undefined)}>}
 */
WebGLCallFormatter.EnumsInfo = [
    {"aname": "activeTexture", "enum": [0]},
    {"aname": "bindBuffer", "enum": [0]},
    {"aname": "bindFramebuffer", "enum": [0]},
    {"aname": "bindRenderbuffer", "enum": [0]},
    {"aname": "bindTexture", "enum": [0]},
    {"aname": "blendEquation", "enum": [0]},
    {"aname": "blendEquationSeparate", "enum": [0, 1]},
    {"aname": "blendFunc", "enum": [0, 1], "hints": ["ZERO", "ONE"]},
    {"aname": "blendFuncSeparate", "enum": [0, 1, 2, 3], "hints": ["ZERO", "ONE"]},
    {"aname": "bufferData", "enum": [0, 2]},
    {"aname": "bufferSubData", "enum": [0]},
    {"aname": "checkFramebufferStatus", "enum": [0], "returnType": "enum"},
    {"aname": "clear", "bitfield": [0]},
    {"aname": "compressedTexImage2D", "enum": [0, 2]},
    {"aname": "compressedTexSubImage2D", "enum": [0, 6]},
    {"aname": "copyTexImage2D", "enum": [0, 2]},
    {"aname": "copyTexSubImage2D", "enum": [0]},
    {"aname": "createShader", "enum": [0]},
    {"aname": "cullFace", "enum": [0]},
    {"aname": "depthFunc", "enum": [0]},
    {"aname": "disable", "enum": [0]},
    {"aname": "drawArrays", "enum": [0], "hints": ["POINTS", "LINES"]},
    {"aname": "drawElements", "enum": [0, 2], "hints": ["POINTS", "LINES"]},
    {"aname": "enable", "enum": [0]},
    {"aname": "framebufferRenderbuffer", "enum": [0, 1, 2]},
    {"aname": "framebufferTexture2D", "enum": [0, 1, 2]},
    {"aname": "frontFace", "enum": [0]},
    {"aname": "generateMipmap", "enum": [0]},
    {"aname": "getBufferParameter", "enum": [0, 1]},
    {"aname": "getError", "hints": ["NO_ERROR"], "returnType": "enum"},
    {"aname": "getFramebufferAttachmentParameter", "enum": [0, 1, 2]},
    {"aname": "getParameter", "enum": [0]},
    {"aname": "getProgramParameter", "enum": [1]},
    {"aname": "getRenderbufferParameter", "enum": [0, 1]},
    {"aname": "getShaderParameter", "enum": [1]},
    {"aname": "getShaderPrecisionFormat", "enum": [0, 1]},
    {"aname": "getTexParameter", "enum": [0, 1], "returnType": "enum"},
    {"aname": "getVertexAttrib", "enum": [1]},
    {"aname": "getVertexAttribOffset", "enum": [1]},
    {"aname": "hint", "enum": [0, 1]},
    {"aname": "isEnabled", "enum": [0]},
    {"aname": "pixelStorei", "enum": [0]},
    {"aname": "readPixels", "enum": [4, 5]},
    {"aname": "renderbufferStorage", "enum": [0, 1]},
    {"aname": "stencilFunc", "enum": [0]},
    {"aname": "stencilFuncSeparate", "enum": [0, 1]},
    {"aname": "stencilMaskSeparate", "enum": [0]},
    {"aname": "stencilOp", "enum": [0, 1, 2], "hints": ["ZERO", "ONE"]},
    {"aname": "stencilOpSeparate", "enum": [0, 1, 2, 3], "hints": ["ZERO", "ONE"]},
    {"aname": "texParameterf", "enum": [0, 1, 2]},
    {"aname": "texParameteri", "enum": [0, 1, 2]},
    {"aname": "texImage2D", "enum": [0, 2, 6, 7]},
    {"aname": "texImage2D", "enum": [0, 2, 3, 4]},
    {"aname": "texSubImage2D", "enum": [0, 6, 7]},
    {"aname": "texSubImage2D", "enum": [0, 4, 5]},
    {"aname": "vertexAttribPointer", "enum": [2]}
];

WebGLCallFormatter.prototype = {
    /**
     * @override
     * @param {!ReplayableCall} replayableCall
     * @param {string=} objectGroup
     * @return {!Object}
     */
    formatCall: function(replayableCall, objectGroup)
    {
        var result = CallFormatter.prototype.formatCall.call(this, replayableCall, objectGroup);
        if (!result.functionName)
            return result;
        var enumsInfo = this._findEnumsInfo(replayableCall);
        if (!enumsInfo)
            return result;
        var enumArgsIndexes = enumsInfo["enum"] || [];
        for (var i = 0, n = enumArgsIndexes.length; i < n; ++i) {
            var index = enumArgsIndexes[i];
            var callArgument = result.arguments[index];
            this._formatEnumValue(callArgument, enumsInfo["hints"]);
        }
        var bitfieldArgsIndexes = enumsInfo["bitfield"] || [];
        for (var i = 0, n = bitfieldArgsIndexes.length; i < n; ++i) {
            var index = bitfieldArgsIndexes[i];
            var callArgument = result.arguments[index];
            this._formatEnumBitmaskValue(callArgument, enumsInfo["hints"]);
        }
        if (enumsInfo.returnType === "enum")
            this._formatEnumValue(result.result, enumsInfo["hints"]);
        else if (enumsInfo.returnType === "bitfield")
            this._formatEnumBitmaskValue(result.result, enumsInfo["hints"]);
        return result;
    },

    /**
     * @override
     * @param {string} name
     * @return {?string}
     */
    enumValueForName: function(name)
    {
        this._initialize();
        if (name in this._enumNameToValue)
            return "" + this._enumNameToValue[name];
        return null;
    },

    /**
     * @override
     * @param {number} value
     * @param {!Array.<string>=} options
     * @return {?string}
     */
    enumNameForValue: function(value, options)
    {
        this._initialize();
        options = options || [];
        for (var i = 0, n = options.length; i < n; ++i) {
            if (this._enumNameToValue[options[i]] === value)
                return options[i];
        }
        var names = this._enumValueToNames[value];
        if (!names || names.length !== 1)
            return null;
        return names[0];
    },

    /**
     * @param {!ReplayableCall} replayableCall
     * @return {?Object}
     */
    _findEnumsInfo: function(replayableCall)
    {
        function findMaxArgumentIndex(enumsInfo)
        {
            var result = -1;
            var enumArgsIndexes = enumsInfo["enum"] || [];
            for (var i = 0, n = enumArgsIndexes.length; i < n; ++i)
                result = Math.max(result, enumArgsIndexes[i]);
            var bitfieldArgsIndexes = enumsInfo["bitfield"] || [];
            for (var i = 0, n = bitfieldArgsIndexes.length; i < n; ++i)
                result = Math.max(result, bitfieldArgsIndexes[i]);
            return result;
        }

        var result = null;
        for (var i = 0, enumsInfo; enumsInfo = WebGLCallFormatter.EnumsInfo[i]; ++i) {
            if (enumsInfo["aname"] !== replayableCall.functionName())
                continue;
            var argsCount = replayableCall.args().length;
            var maxArgumentIndex = findMaxArgumentIndex(enumsInfo);
            if (maxArgumentIndex >= argsCount)
                continue;
            // To resolve ambiguity (see texImage2D, texSubImage2D) choose description with max argument indexes.
            if (!result || findMaxArgumentIndex(result) < maxArgumentIndex)
                result = enumsInfo;
        }
        return result;
    },

    /**
     * @param {?CanvasAgent.CallArgument|undefined} callArgument
     * @param {!Array.<string>=} options
     */
    _formatEnumValue: function(callArgument, options)
    {
        if (!callArgument || isNaN(callArgument.description))
            return;
        this._initialize();
        var value = +callArgument.description;
        var enumName = this.enumNameForValue(value, options);
        if (enumName)
            callArgument.enumName = enumName;
    },

    /**
     * @param {?CanvasAgent.CallArgument|undefined} callArgument
     * @param {!Array.<string>=} options
     */
    _formatEnumBitmaskValue: function(callArgument, options)
    {
        if (!callArgument || isNaN(callArgument.description))
            return;
        this._initialize();
        var value = +callArgument.description;
        options = options || [];
        /** @type {!Array.<string>} */
        var result = [];
        for (var i = 0, n = options.length; i < n; ++i) {
            var bitValue = this._enumNameToValue[options[i]] || 0;
            if (value & bitValue) {
                result.push(options[i]);
                value &= ~bitValue;
            }
        }
        while (value) {
            var nextValue = value & (value - 1);
            var bitValue = value ^ nextValue;
            var names = this._enumValueToNames[bitValue];
            if (!names || names.length !== 1) {
                console.warn("Ambiguous WebGL enum names for value " + bitValue + ": " + names);
                return;
            }
            result.push(names[0]);
            value = nextValue;
        }
        result.sort();
        callArgument.enumName = result.join(" | ");
    },

    _initialize: function()
    {
        if (this._enumNameToValue)
            return;

        /** @type {!Object.<string, number>} */
        this._enumNameToValue = Object.create(null);
        /** @type {!Object.<number, !Array.<string>>} */
        this._enumValueToNames = Object.create(null);

        /**
         * @param {?Object} obj
         * @this WebGLCallFormatter
         */
        function iterateWebGLEnums(obj)
        {
            if (!obj)
                return;
            for (var property in obj) {
                if (TypeUtils.isEnumPropertyName(property, obj)) {
                    var value = /** @type {number} */ (obj[property]);
                    this._enumNameToValue[property] = value;
                    var names = this._enumValueToNames[value];
                    if (names) {
                        if (names.indexOf(property) === -1)
                            names.push(property);
                    } else
                        this._enumValueToNames[value] = [property];
                }
            }
        }

        /**
         * @param {!Array.<string>} values
         * @return {string}
         */
        function commonSubstring(values)
        {
            var length = values.length;
            for (var i = 0; i < length; ++i) {
                for (var j = 0; j < length; ++j) {
                    if (values[j].indexOf(values[i]) === -1)
                        break;
                }
                if (j === length)
                    return values[i];
            }
            return "";
        }

        var gl = this._createUninstrumentedWebGLRenderingContext();
        iterateWebGLEnums.call(this, gl);

        var extensions = gl.getSupportedExtensions() || [];
        for (var i = 0, n = extensions.length; i < n; ++i)
            iterateWebGLEnums.call(this, gl.getExtension(extensions[i]));

        // Sort to get rid of ambiguity.
        for (var value in this._enumValueToNames) {
            var numericValue = Number(value);
            var names = this._enumValueToNames[numericValue];
            if (names.length > 1) {
                // Choose one enum name if possible. For example:
                //   [BLEND_EQUATION, BLEND_EQUATION_RGB] => BLEND_EQUATION
                //   [COLOR_ATTACHMENT0, COLOR_ATTACHMENT0_WEBGL] => COLOR_ATTACHMENT0
                var common = commonSubstring(names);
                if (common)
                    this._enumValueToNames[numericValue] = [common];
                else
                    this._enumValueToNames[numericValue] = names.sort();
            }
        }
    },

    /**
     * @return {?WebGLRenderingContext}
     */
    _createUninstrumentedWebGLRenderingContext: function()
    {
        var canvas = /** @type {!HTMLCanvasElement} */ (inspectedWindow.document.createElement("canvas"));
        var contextIds = ["experimental-webgl", "webkit-3d", "3d"];
        for (var i = 0, contextId; contextId = contextIds[i]; ++i) {
            var context = canvas.getContext(contextId);
            if (context)
                return /** @type {!WebGLRenderingContext} */ (Resource.wrappedObject(context));
        }
        return null;
    },

    __proto__: CallFormatter.prototype
}

CallFormatter.register("CanvasRenderingContext2D", new CallFormatter(CanvasRenderingContext2DResource.DrawingMethods));
CallFormatter.register("WebGLRenderingContext", new WebGLCallFormatter(WebGLRenderingContextResource.DrawingMethods));

/**
 * @constructor
 */
function TraceLog()
{
    /** @type {!Array.<!ReplayableCall>} */
    this._replayableCalls = [];
    /** @type {!Cache.<!ReplayableResource>} */
    this._replayablesCache = new Cache();
    /** @type {!Object.<number, boolean>} */
    this._frameEndCallIndexes = {};
    /** @type {!Object.<number, boolean>} */
    this._resourcesCreatedInThisTraceLog = {};
}

TraceLog.prototype = {
    /**
     * @return {number}
     */
    size: function()
    {
        return this._replayableCalls.length;
    },

    /**
     * @return {!Array.<!ReplayableCall>}
     */
    replayableCalls: function()
    {
        return this._replayableCalls;
    },

    /**
     * @param {number} id
     * @return {!ReplayableResource|undefined}
     */
    replayableResource: function(id)
    {
        return this._replayablesCache.get(id);
    },

    /**
     * @param {number} resourceId
     * @return {boolean}
     */
    createdInThisTraceLog: function(resourceId)
    {
        return !!this._resourcesCreatedInThisTraceLog[resourceId];
    },

    /**
     * @param {!Resource} resource
     */
    captureResource: function(resource)
    {
        resource.toReplayable(this._replayablesCache);
    },

    /**
     * @param {!Call} call
     */
    addCall: function(call)
    {
        var resource = Resource.forObject(call.result());
        if (resource && !this._replayablesCache.has(resource.id()))
            this._resourcesCreatedInThisTraceLog[resource.id()] = true;
        this._replayableCalls.push(call.toReplayable(this._replayablesCache));
    },

    addFrameEndMark: function()
    {
        var index = this._replayableCalls.length - 1;
        if (index >= 0)
            this._frameEndCallIndexes[index] = true;
    },

    /**
     * @param {number} index
     * @return {boolean}
     */
    isFrameEndCallAt: function(index)
    {
        return !!this._frameEndCallIndexes[index];
    }
}

/**
 * @constructor
 * @param {!TraceLog} traceLog
 */
function TraceLogPlayer(traceLog)
{
    /** @type {!TraceLog} */
    this._traceLog = traceLog;
    /** @type {number} */
    this._nextReplayStep = 0;
    /** @type {!Cache.<!Resource>} */
    this._replayWorldCache = new Cache();
}

TraceLogPlayer.prototype = {
    /**
     * @return {!TraceLog}
     */
    traceLog: function()
    {
        return this._traceLog;
    },

    /**
     * @param {number} id
     * @return {!Resource|undefined}
     */
    replayWorldResource: function(id)
    {
        return this._replayWorldCache.get(id);
    },

    /**
     * @return {number}
     */
    nextReplayStep: function()
    {
        return this._nextReplayStep;
    },

    reset: function()
    {
        this._nextReplayStep = 0;
        this._replayWorldCache.reset();
    },

    /**
     * @param {number} stepNum
     * @return {{replayTime:number, lastCall:(!Call)}}
     */
    stepTo: function(stepNum)
    {
        stepNum = Math.min(stepNum, this._traceLog.size() - 1);
        console.assert(stepNum >= 0);
        if (this._nextReplayStep > stepNum)
            this.reset();

        // Replay the calls' arguments first to warm-up, before measuring the actual replay time.
        this._replayCallArguments(stepNum);

        var replayableCalls = this._traceLog.replayableCalls();
        var replayedCalls = [];
        replayedCalls.length = stepNum - this._nextReplayStep + 1;

        var beforeTime = TypeUtils.now();
        for (var i = 0; this._nextReplayStep <= stepNum; ++this._nextReplayStep, ++i)
            replayedCalls[i] = replayableCalls[this._nextReplayStep].replay(this._replayWorldCache);
        var replayTime = Math.max(0, TypeUtils.now() - beforeTime);

        for (var i = 0, call; call = replayedCalls[i]; ++i)
            call.resource().onCallReplayed(call);

        return {
            replayTime: replayTime,
            lastCall: replayedCalls[replayedCalls.length - 1]
        };
    },

    /**
     * @param {number} stepNum
     */
    _replayCallArguments: function(stepNum)
    {
        /**
         * @param {*} obj
         * @this {TraceLogPlayer}
         */
        function replayIfNotCreatedInThisTraceLog(obj)
        {
            if (!(obj instanceof ReplayableResource))
                return;
            var replayableResource = /** @type {!ReplayableResource} */ (obj);
            if (!this._traceLog.createdInThisTraceLog(replayableResource.id()))
                replayableResource.replay(this._replayWorldCache)
        }
        var replayableCalls = this._traceLog.replayableCalls();
        for (var i = this._nextReplayStep; i <= stepNum; ++i) {
            replayIfNotCreatedInThisTraceLog.call(this, replayableCalls[i].replayableResource());
            replayIfNotCreatedInThisTraceLog.call(this, replayableCalls[i].result());
            replayableCalls[i].args().forEach(replayIfNotCreatedInThisTraceLog.bind(this));
        }
    }
}

/**
 * @constructor
 */
function ResourceTrackingManager()
{
    this._capturing = false;
    this._stopCapturingOnFrameEnd = false;
    this._lastTraceLog = null;
}

ResourceTrackingManager.prototype = {
    /**
     * @return {boolean}
     */
    capturing: function()
    {
        return this._capturing;
    },

    /**
     * @return {?TraceLog}
     */
    lastTraceLog: function()
    {
        return this._lastTraceLog;
    },

    /**
     * @param {!Resource} resource
     */
    registerResource: function(resource)
    {
        resource.setManager(this);
    },

    startCapturing: function()
    {
        if (!this._capturing)
            this._lastTraceLog = new TraceLog();
        this._capturing = true;
        this._stopCapturingOnFrameEnd = false;
    },

    /**
     * @param {!TraceLog=} traceLog
     */
    stopCapturing: function(traceLog)
    {
        if (traceLog && this._lastTraceLog !== traceLog)
            return;
        this._capturing = false;
        this._stopCapturingOnFrameEnd = false;
        if (this._lastTraceLog)
            this._lastTraceLog.addFrameEndMark();
    },

    /**
     * @param {!TraceLog} traceLog
     */
    dropTraceLog: function(traceLog)
    {
        this.stopCapturing(traceLog);
        if (this._lastTraceLog === traceLog)
            this._lastTraceLog = null;
    },

    captureFrame: function()
    {
        this._lastTraceLog = new TraceLog();
        this._capturing = true;
        this._stopCapturingOnFrameEnd = true;
    },

    /**
     * @param {!Resource} resource
     * @param {!Array|!Arguments} args
     */
    captureArguments: function(resource, args)
    {
        if (!this._capturing)
            return;
        this._lastTraceLog.captureResource(resource);
        for (var i = 0, n = args.length; i < n; ++i) {
            var res = Resource.forObject(args[i]);
            if (res)
                this._lastTraceLog.captureResource(res);
        }
    },

    /**
     * @param {!Call} call
     */
    captureCall: function(call)
    {
        if (!this._capturing)
            return;
        this._lastTraceLog.addCall(call);
    },

    markFrameEnd: function()
    {
        if (!this._lastTraceLog)
            return;
        this._lastTraceLog.addFrameEndMark();
        if (this._stopCapturingOnFrameEnd && this._lastTraceLog.size())
            this.stopCapturing(this._lastTraceLog);
    }
}

/**
 * @constructor
 */
var InjectedCanvasModule = function()
{
    /** @type {!ResourceTrackingManager} */
    this._manager = new ResourceTrackingManager();
    /** @type {number} */
    this._lastTraceLogId = 0;
    /** @type {!Object.<string, !TraceLog>} */
    this._traceLogs = {};
    /** @type {!Object.<string, !TraceLogPlayer>} */
    this._traceLogPlayers = {};
}

InjectedCanvasModule.prototype = {
    /**
     * @param {!WebGLRenderingContext} glContext
     * @return {!Object}
     */
    wrapWebGLContext: function(glContext)
    {
        var resource = Resource.forObject(glContext) || new WebGLRenderingContextResource(glContext);
        this._manager.registerResource(resource);
        return resource.proxyObject();
    },

    /**
     * @param {!CanvasRenderingContext2D} context
     * @return {!Object}
     */
    wrapCanvas2DContext: function(context)
    {
        var resource = Resource.forObject(context) || new CanvasRenderingContext2DResource(context);
        this._manager.registerResource(resource);
        return resource.proxyObject();
    },

    /**
     * @return {!CanvasAgent.TraceLogId}
     */
    captureFrame: function()
    {
        return this._callStartCapturingFunction(this._manager.captureFrame);
    },

    /**
     * @return {!CanvasAgent.TraceLogId}
     */
    startCapturing: function()
    {
        return this._callStartCapturingFunction(this._manager.startCapturing);
    },

    markFrameEnd: function()
    {
        this._manager.markFrameEnd();
    },

    /**
     * @param {function(this:ResourceTrackingManager)} func
     * @return {!CanvasAgent.TraceLogId}
     */
    _callStartCapturingFunction: function(func)
    {
        var oldTraceLog = this._manager.lastTraceLog();
        func.call(this._manager);
        var traceLog = /** @type {!TraceLog} */ (this._manager.lastTraceLog());
        if (traceLog === oldTraceLog) {
            for (var id in this._traceLogs) {
                if (this._traceLogs[id] === traceLog)
                    return id;
            }
        }
        var id = this._makeTraceLogId();
        this._traceLogs[id] = traceLog;
        return id;
    },

    /**
     * @param {!CanvasAgent.TraceLogId} id
     */
    stopCapturing: function(id)
    {
        var traceLog = this._traceLogs[id];
        if (traceLog)
            this._manager.stopCapturing(traceLog);
    },

    /**
     * @param {!CanvasAgent.TraceLogId} id
     */
    dropTraceLog: function(id)
    {
        var traceLog = this._traceLogs[id];
        if (traceLog)
            this._manager.dropTraceLog(traceLog);
        delete this._traceLogs[id];
        delete this._traceLogPlayers[id];
        injectedScript.releaseObjectGroup(id);
    },

    /**
     * @param {!CanvasAgent.TraceLogId} id
     * @param {number=} startOffset
     * @param {number=} maxLength
     * @return {!CanvasAgent.TraceLog|string}
     */
    traceLog: function(id, startOffset, maxLength)
    {
        var traceLog = this._traceLogs[id];
        if (!traceLog)
            return "Error: Trace log with the given ID not found.";

        // Ensure last call ends a frame.
        traceLog.addFrameEndMark();

        var replayableCalls = traceLog.replayableCalls();
        if (typeof startOffset !== "number")
            startOffset = 0;
        if (typeof maxLength !== "number")
            maxLength = replayableCalls.length;

        var fromIndex = Math.max(0, startOffset);
        var toIndex = Math.min(replayableCalls.length - 1, fromIndex + maxLength - 1);

        var alive = this._manager.capturing() && this._manager.lastTraceLog() === traceLog;
        var result = {
            id: id,
            /** @type {!Array.<!CanvasAgent.Call>} */
            calls: [],
            /** @type {!Array.<!CanvasAgent.CallArgument>} */
            contexts: [],
            alive: alive,
            startOffset: fromIndex,
            totalAvailableCalls: replayableCalls.length
        };
        /** @type {!Object.<string, boolean>} */
        var contextIds = {};
        for (var i = fromIndex; i <= toIndex; ++i) {
            var call = replayableCalls[i];
            var resource = call.replayableResource();
            var contextResource = resource.contextResource();
            var stackTrace = call.stackTrace();
            var callFrame = stackTrace ? stackTrace.callFrame(0) || {} : {};
            var item = CallFormatter.forResource(resource).formatCall(call);
            item.contextId = CallFormatter.makeStringResourceId(contextResource.id());
            item.sourceURL = callFrame.sourceURL;
            item.lineNumber = callFrame.lineNumber;
            item.columnNumber = callFrame.columnNumber;
            item.isFrameEndCall = traceLog.isFrameEndCallAt(i);
            result.calls.push(item);
            if (!contextIds[item.contextId]) {
                contextIds[item.contextId] = true;
                result.contexts.push(CallFormatter.forResource(resource).formatValue(contextResource));
            }
        }
        return result;
    },

    /**
     * @param {!CanvasAgent.TraceLogId} traceLogId
     * @param {number} stepNo
     * @return {{resourceState: !CanvasAgent.ResourceState, replayTime: number}|string}
     */
    replayTraceLog: function(traceLogId, stepNo)
    {
        var traceLog = this._traceLogs[traceLogId];
        if (!traceLog)
            return "Error: Trace log with the given ID not found.";
        this._traceLogPlayers[traceLogId] = this._traceLogPlayers[traceLogId] || new TraceLogPlayer(traceLog);
        injectedScript.releaseObjectGroup(traceLogId);

        var replayResult = this._traceLogPlayers[traceLogId].stepTo(stepNo);
        var resource = replayResult.lastCall.resource();
        var dataURL = resource.toDataURL();
        if (!dataURL) {
            resource = resource.contextResource();
            dataURL = resource.toDataURL();
        }
        return {
            resourceState: this._makeResourceState(resource.id(), traceLogId, resource, dataURL),
            replayTime: replayResult.replayTime
        };
    },

    /**
     * @param {!CanvasAgent.TraceLogId} traceLogId
     * @param {!CanvasAgent.ResourceId} stringResourceId
     * @return {!CanvasAgent.ResourceState|string}
     */
    resourceState: function(traceLogId, stringResourceId)
    {
        var traceLog = this._traceLogs[traceLogId];
        if (!traceLog)
            return "Error: Trace log with the given ID not found.";

        var parsedStringId1 = this._parseStringId(traceLogId);
        var parsedStringId2 = this._parseStringId(stringResourceId);
        if (parsedStringId1.injectedScriptId !== parsedStringId2.injectedScriptId)
            return "Error: Both IDs must point to the same injected script.";

        var resourceId = parsedStringId2.resourceId;
        if (!resourceId)
            return "Error: Wrong resource ID: " + stringResourceId;

        var traceLogPlayer = this._traceLogPlayers[traceLogId];
        var resource = traceLogPlayer && traceLogPlayer.replayWorldResource(resourceId);
        return this._makeResourceState(resourceId, traceLogId, resource);
    },

    /**
     * @param {!CanvasAgent.TraceLogId} traceLogId
     * @param {number} callIndex
     * @param {number} argumentIndex
     * @param {string} objectGroup
     * @return {{result:(!RuntimeAgent.RemoteObject|undefined), resourceState:(!CanvasAgent.ResourceState|undefined)}|string}
     */
    evaluateTraceLogCallArgument: function(traceLogId, callIndex, argumentIndex, objectGroup)
    {
        var traceLog = this._traceLogs[traceLogId];
        if (!traceLog)
            return "Error: Trace log with the given ID not found.";

        var replayableCall = traceLog.replayableCalls()[callIndex];
        if (!replayableCall)
            return "Error: No call found at index " + callIndex;

        var value;
        if (replayableCall.isPropertySetter())
            value = replayableCall.propertyValue();
        else if (argumentIndex === -1)
            value = replayableCall.result();
        else {
            var args = replayableCall.args();
            if (argumentIndex < 0 || argumentIndex >= args.length)
                return "Error: No argument found at index " + argumentIndex + " for call at index " + callIndex;
            value = args[argumentIndex];
        }

        if (value instanceof ReplayableResource) {
            var traceLogPlayer = this._traceLogPlayers[traceLogId];
            var resource = traceLogPlayer && traceLogPlayer.replayWorldResource(value.id());
            var resourceState = this._makeResourceState(value.id(), traceLogId, resource);
            return { resourceState: resourceState };
        }

        var remoteObject = injectedScript.wrapObject(value, objectGroup, true, false);
        return { result: remoteObject };
    },

    /**
     * @return {!CanvasAgent.TraceLogId}
     */
    _makeTraceLogId: function()
    {
        return "{\"injectedScriptId\":" + injectedScriptId + ",\"traceLogId\":" + (++this._lastTraceLogId) + "}";
    },

    /**
     * @param {number} resourceId
     * @param {!CanvasAgent.TraceLogId} traceLogId
     * @param {?Resource=} resource
     * @param {string=} overrideImageURL
     * @return {!CanvasAgent.ResourceState}
     */
    _makeResourceState: function(resourceId, traceLogId, resource, overrideImageURL)
    {
        var result = {
            id: CallFormatter.makeStringResourceId(resourceId),
            traceLogId: traceLogId
        };
        if (resource) {
            result.imageURL = overrideImageURL || resource.toDataURL();
            result.descriptors = CallFormatter.forResource(resource).formatResourceStateDescriptors(resource.currentState(), traceLogId);
        }
        return result;
    },

    /**
     * @param {string} stringId
     * @return {{injectedScriptId: number, traceLogId: ?number, resourceId: ?number}}
     */
    _parseStringId: function(stringId)
    {
        return InjectedScriptHost.evaluate("(" + stringId + ")");
    }
}

var injectedCanvasModule = new InjectedCanvasModule();
return injectedCanvasModule;

})
