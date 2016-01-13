/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

module("treestatus");

var openTreeJson = {
    "username": "erg@chromium.org",
    "date": "2013-10-14 20:22:00.887390",
    "message": "Tree is open",
    "can_commit_freely": true,
    "general_state": "open"
};

var closedTreeJson = {
    "username": "ojan@chromium.org",
    "date": "2013-10-14 20:32:09.642350",
    "message": "Tree is closed",
    "can_commit_freely": false,
    "general_state": "closed"
};

test('urlByName', 3, function() {
    equal(treestatus.urlByName('blink'), 'http://blink-status.appspot.com/current?format=json');
    equal(treestatus.urlByName('chromium'), 'http://chromium-status.appspot.com/current?format=json');
    equal(treestatus.urlByName('foo'), null);
});

asyncTest('fetchTreeStatus', 4, function() {
    var simulator = new NetworkSimulator();

    simulator.json = function(url)
    {
        if (url.indexOf('closed') != -1)
            return Promise.resolve(closedTreeJson);
        else
            return Promise.resolve(openTreeJson);
    };

    var span = document.createElement('span');
    simulator.runTest(function() {
        treestatus.fetchTreeStatus('http://opentree', span)
            .then(function(result) {
                equal(span.textContent, 'OPEN');

                span = document.createElement('span');
                simulator.runTest(function() {
                    treestatus.fetchTreeStatus('http://closedtree', span)
                        .then(function() {
                            equal(span.textContent, 'Tree is closed by ojan@chromium.org');
                            start();
                        });
                });
            });
    });
});

})();
