// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple, incomplete implementation of Fetch, intended to facilitate end
// to end serviceworker testing.

// See http://fetch.spec.whatwg.org/#fetch-method

(function (global) {
    var _castToRequest = function (item) {
        if (typeof item === 'string') {
            item = new Request({
                url: item
            });
        }
        return item;
    };

    // FIXME: Support init argument to fetch.
    var fetch = function(request) {
        request = _castToRequest(request);

        return new Promise(function(resolve, reject) {
            // FIXME: Use extra headers from |request|.
            var xhr = new XMLHttpRequest();
            xhr.open(request.method, request.url, true /* async */);
            xhr.send(null);
            xhr.onreadystatechange = function() {
                if (xhr.readyState !== 4) return;

                var response = new Response({
                    status: xhr.status,
                    statusText: xhr.statusText,
                    // FIXME: Set response.headers.
                    // FIXME: Set response.method when available.
                    // FIXME: Set response.url when available.
                    // FIXME: Set response.body when available.
                });

                if (xhr.status === 200) {
                    resolve(response);
                } else {
                    reject(response);
                }
            };
        });
    };

    global.fetch = global.fetch || fetch;
}(self));  // window or worker global scope
