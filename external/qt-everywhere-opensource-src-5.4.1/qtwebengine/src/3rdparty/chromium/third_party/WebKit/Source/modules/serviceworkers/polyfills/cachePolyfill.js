// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple, incomplete implementation of the Cache API, intended to facilitate
// end to end serviceworker testing.

// See https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html#cache-objects

// FIXME: Support AbstractResponse/OpaqueResponse correctly.
// FIXME: Serialize the cache.
// FIXME: Bind all function references.
(function(global) {
    var _castToRequest = function(item) {
        if (typeof item === 'string') {
            item = new Request({
                url: item,
            });
        }
        return item;
    };

    var Cache = function() {
        // An object containing a property for each HTTP fetch method. Those
        // referenced objects contain a property for each URL, which is the
        // Response.
        this.entriesByMethod = {};
    };

    // FIXME: Should this be in the spec?
    Cache.prototype.keys = function() {
        var that = this;

        var flatten = Array.prototype.concat.apply.bind(Array.prototype.concat, []);

        return Promise.resolve(flatten(
            Object.keys(this.entriesByMethod).map(function(method) {
                return Object.keys(that.entriesByMethod[method]).map(function(url) {
                    return new Request({method: method, url: url});
                });
            })));
    };

    // FIXME: Implement this.
    // FIXME: Should spec rename each --> forEach?
    Cache.prototype.forEach = Promise.reject.bind(Promise, 'Cache.prototype.forEach() not implemented.');
    Cache.prototype.each = Promise.reject.bind(Promise, 'Cache.prototype.each() not implemented.');

    Cache.prototype.put = function(request, response) {
        var that = this;

        return new Promise(function(resolve, reject) {
            request = _castToRequest(request);

            if (!that.entriesByMethod.hasOwnProperty(request.method)) {
                that.entriesByMethod[request.method] = {};
            }

            var entriesByUrl = that.entriesByMethod[request.method];
            entriesByUrl[request.url] = response;

            resolve();
        });
    };

    Cache.prototype.add = function(request) {
        var that = this;
        request = _castToRequest(request);
        return new Promise(function (resolve, reject) {
            fetch(request).then(
                function(response) {
                    that.put(request, response).then(resolve);
                },
                reject);
        });
    };

    // FIXME: Add QueryParams argument.
    Cache.prototype.delete = function(request) {
        request = _castToRequest(request);

        var that = this;
        return new Promise(function(resolve, reject) {
            if (that.entriesByMethod.hasOwnProperty(request.method)) {
                var entriesByUrl = that.entriesByMethod[request.method];
                delete entriesByUrl[request.url];
            }
            resolve();
        });
    };

    // FIXME: Add QueryParams argument.
    Cache.prototype.match = function(request) {
        var that = this;

        return new Promise(function(resolve, reject) {
            request = _castToRequest(request);

            if (!that.entriesByMethod.hasOwnProperty(request.method)) {
                reject('not found');
                return;
            }

            var entriesByUrl = that.entriesByMethod[request.method];

            if (!entriesByUrl.hasOwnProperty(request.url)) {
                reject('not found');
                return;
            }

            var entry = entriesByUrl[request.url];
            resolve(entry);
        });
    };

    // FIXME: Implement this.
    Cache.prototype.matchAll = Promise.reject.bind(Promise, 'Cache.prototype.matchAll not implemented.');

    global.Cache = global.Cache || Cache;
}(self));  // window or worker global scope.
