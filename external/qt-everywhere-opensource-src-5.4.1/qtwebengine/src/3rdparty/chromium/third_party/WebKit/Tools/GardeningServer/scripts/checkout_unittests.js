/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

(function () {

module("checkout");

asyncTest("lastBlinkRollRevision", 0, function() {
    var simulator = new NetworkSimulator();

    var requests = [];
    simulator.get = function(url)
    {
        requests.push([url]);
        return Promise.resolve();
    };

    simulator.ajax = function(options)
    {
        if (options.url.indexOf('/ping') == -1) {
            ok(false, 'Received non-ping ajax request: ' + options.url);
            return Promise.reject('Received non-ping ajax request: ' + options.url);
        }
        return Promise.resolve();
    };

    simulator.runTest(function() {
        checkout.lastBlinkRollRevision(function() {
            ok(true);
        }, function() {
            ok(false, 'Checkout should be available.');
        });
    }).then(function() {
        deepEqual(requests, [
            ["/lastroll"]
        ]);
        start();
    });;
});

asyncTest("rebaseline", 3, function() {
    var simulator = new NetworkSimulator();

    var requests = [];
    simulator.post = function(url, body)
    {
        requests.push([url, body]);
        return Promise.resolve();
    };
    simulator.ajax = function(options)
    {
        if (options.url.indexOf('/ping') == -1) {
            ok(false, 'Received non-ping ajax request: ' + options.url);
            return Promise.reject('Received non-ping ajax request: ' + options.url);
        }
        return Promise.resolve();
    };

    var kExpectedTestNameProgressStack = [
        'fast/test.html',
        'another/test.svg',
        'another/test.svg', // This is the first one.
    ];

    simulator.runTest(function() {
        checkout.rebaseline([{
            'builderName': 'WebKit Linux',
            'testName': 'another/test.svg',
            'failureTypeList': ['IMAGE'],
        }, {
            'builderName': 'WebKit Mac10.6',
            'testName': 'another/test.svg',
            'failureTypeList': ['IMAGE', 'TEXT', 'IMAGE+TEXT'],
        }, {
            'builderName': 'Webkit Win7',
            'testName': 'fast/test.html',
            'failureTypeList': ['IMAGE+TEXT'],
        }], function(failureInfo) {
            equals(failureInfo.testName, kExpectedTestNameProgressStack.pop());
        }, function() {
            ok(false, 'There are no debug bots in the list');
        }).catch().then(function() {
            ok(true);
        }, function() {
            ok(false, 'Checkout should be available.');
        });
    }).then(function() {

        deepEqual(requests, [
            ["/rebaselineall",
             JSON.stringify({
                 "another/test.svg": {
                     "WebKit Linux": ["png"],
                     "WebKit Mac10.6": ["png","txt"]},
                 "fast/test.html": {
                     "Webkit Win7": ["txt","png"]
                 }})]
        ]);
        start();
    });
});

asyncTest("rebaseline-debug-bot", 4, function() {
    var simulator = new NetworkSimulator();
    simulator.post = function(url, body)
    {
        return Promise.resolve();
    };
    simulator.ajax = function(options)
    {
        return Promise.resolve();
    };

    simulator.runTest(function() {
        checkout.rebaseline([{
            'builderName': 'WebKit Linux (dbg)',
            'testName': 'another/test.svg',
            'failureTypeList': ['IMAGE'],
        }], function(failureInfo) {
            ok(true);
        }, function(failureInfo) {
            ok(true);
        }).then(function() {
            ok(true);
        }, function() {
            ok(false);
        });
    }).then(start);
});

})();
