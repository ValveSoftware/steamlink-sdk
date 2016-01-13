// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//         * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//         * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

module('results');

test('results.testCounts', 2, function() {
    var failuresByType = {
        'PASS': [2, 3],
        'TIMEOUT': [4, 2],
        'CRASH': [1, 0]
    }

    deepEqual(results.testCounts(failuresByType), {
        totalTests: [7, 5],
        totalFailingTests: [5, 2]
    });

    failuresByType = {
        'PASS': [2, 3]
    }
    deepEqual(results.testCounts(failuresByType), {
        totalTests: [2, 3],
        totalFailingTests: [0, 0]
    });
});

test('results.determineFlakiness', 10, function() {
    var failureMap = {
        'C': 'CRASH',
        'P': 'PASS',
        'I': 'IMAGE',
        'T': 'TIMEOUT',
        'N':'NO DATA',
        'Y':'NOTRUN',
        'X':'SKIP'
    };
    var out = {};

    var inputResults = [[1, 'P']];
    results.determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 0);

    inputResults = [[1, 'P'], [1, 'C']];
    results.determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 1);

    inputResults = [[1, 'P'], [1, 'C'], [1, 'P']];
    results.determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, true);
    equal(out.flipCount, 2);

    inputResults = [[1, 'P'], [1, 'C'], [1, 'P'], [1, 'C']];
    results.determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, true);
    equal(out.flipCount, 3);

    inputResults = [[1, 'P'], [1, 'Y'], [1, 'N'], [1, 'X'], [1, 'P'], [1, 'Y'], [1, 'N'], [1, 'X'], [1, 'C']];
    results.determineFlakiness(failureMap, inputResults, out);
    equal(out.isFlaky, false);
    equal(out.flipCount, 1);
});
