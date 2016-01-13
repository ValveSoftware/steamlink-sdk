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

var results = results || {};

(function() {

// Keys in the JSON files.
results.NUM_FAILURES_BY_TYPE = 'num_failures_by_type';
results.FAILURE_MAP = 'failure_map';
results.CHROME_REVISIONS = 'chromeRevision';
results.BLINK_REVISIONS = 'blinkRevision';
results.TIMESTAMPS = 'secondsSinceEpoch';
results.BUILD_NUMBERS = 'buildNumbers';
results.TESTS = 'tests';

// Failure types.
results.PASS = 'PASS';
results.NO_DATA = 'NO DATA';
results.SKIP = 'SKIP';
results.NOTRUN = 'NOTRUN';
results.WONTFIX = 'WONTFIX';

// FIXME: Create a ResultsJson class or something similar that abstracts out the JSON
// data format. Code outside this class shouldn't know about the guts of the JSON format.

// Enum for indexing into the run-length encoded results in the JSON files.
// 0 is where the count is length is stored. 1 is the value.
results.RLE = {
    LENGTH: 0,
    VALUE: 1
}

var NON_FAILURE_TYPES = [results.PASS, results.NO_DATA, results.SKIP, results.NOTRUN, results.WONTFIX];

results.isFailingResult = function(failureMap, failureType)
{
    return NON_FAILURE_TYPES.indexOf(failureMap[failureType]) == -1;
}

results.testCounts = function(failuresByType)
{
    var countData = {
        totalTests: [],
        totalFailingTests: []
    };

    for (var failureType in failuresByType) {
        if (failureType == results.WONTFIX)
            continue;

        var failures = failuresByType[failureType];
        failures.forEach(function(count, index) {
            if (!countData.totalTests[index]) {
                countData.totalTests[index] = 0;
                countData.totalFailingTests[index] = 0;
            }

            countData.totalTests[index] += count;
            if (failureType != results.PASS)
                countData.totalFailingTests[index] += count;
        });
    }
    return countData;
}

results.determineFlakiness = function(failureMap, testResults, out)
{
    // FIXME: Ideally this heuristic would be a bit smarter and not consider
    // all passes, followed by a few consecutive failures, followed by all passes
    // to be flakiness since that's more likely the test actually failing for a
    // few runs due to a commit.
    var FAILURE_TYPES_TO_IGNORE = [results.NOTRUN, results.NO_DATA, results.SKIP];
    var flipCount = 0;
    var mostRecentNonIgnorableFailureType;

    for (var i = 0; i < testResults.length; i++) {
        var result = testResults[i][results.RLE.VALUE];
        var failureType = failureMap[result];
        if (failureType != mostRecentNonIgnorableFailureType && FAILURE_TYPES_TO_IGNORE.indexOf(failureType) == -1) {
            if (mostRecentNonIgnorableFailureType)
                flipCount++;
            mostRecentNonIgnorableFailureType = failureType;
        }
    }

    out.flipCount = flipCount;
    out.isFlaky = flipCount > 1;
}

})();
