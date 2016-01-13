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

module('overview');

test('getFlakyData', 2, function() {
    var testTypes = ['MockTestType'];

    var failureMap = {
        'T': 'TIMEOUT',
        'C': 'CRASH',
        'P': 'PASS'
    }

    var resultsByTestType = {
        'MockTestType': {
            'MockBuilder1': {
                'tests': {
                    'TestSuite.NotFlaky': {
                        'results': [[1, 'T'], [1, 'C']]
                    },
                    'TestSuite.Flaky': {
                        'results': [[1, 'T'], [1, 'C'], [1, 'T']]
                    },
                    'TestSuite.VeryFlaky': {
                        'results': [[1, 'T'], [1, 'C'], [1, 'T'], [1, 'C'], [1, 'T'], [1, 'C'], [1, 'T']]
                    }
                },
                'num_failures_by_type': {
                    'PASS': [10, 12],
                    'CRASH': [1, 0]
                },
                'failure_map': failureMap
            }
        }
    };

    var flipCountThreshold = 1;
    deepEqual(overview._getFlakyData(testTypes, resultsByTestType, flipCountThreshold), {
        'MockTestType': {
            "flakyBelowThreshold": {
                "TestSuite.Flaky": true,
                "TestSuite.VeryFlaky": true
            },
            'flaky': {
                'TestSuite.Flaky': true,
                'TestSuite.VeryFlaky': true
            },
            'testCount': 11
        }
    })


    flipCountThreshold = 5;
    deepEqual(overview._getFlakyData(testTypes, resultsByTestType, flipCountThreshold), {
        'MockTestType': {
            "flakyBelowThreshold": {
                "TestSuite.VeryFlaky": true
            },
            'flaky': {
                "TestSuite.Flaky": true,
                'TestSuite.VeryFlaky': true
            },
            'testCount': 11
        }
    })
});

test('htmlForFlakyTests', 6, function() {
    var flakyData = {
        'browser_tests': {
            'testCount': 0,
            "flakyBelowThreshold": {},
            'flaky': {}
        },
        'layout-tests': {
            'testCount': 4,
            "flakyBelowThreshold": {
                'css3/foo.html': true,
                'css3/bar.html': true
            },
            'flaky': {
                'css3/foo.html': true,
                'css3/bar.html': true
            }
        }
    }

    var container = document.createElement('div');
    container.innerHTML = overview._htmlForFlakyTests(flakyData, 'MockGroup');

    // There should only be one row other than the header since browser_tests
    // have testCount of 0.
    ok(!container.querySelectorAll('tr')[2]);

    var firstRow = container.querySelectorAll('tr')[1];
    equal(firstRow.querySelector('td:nth-child(1)').textContent, 'layout-tests');
    equal(firstRow.querySelector('td:nth-child(1) a').hash, '#group=MockGroup&testType=layout-tests&tests=css3/foo.html,css3/bar.html');
    equal(firstRow.querySelector('td:nth-child(2)').textContent, '2 / 4');
    equal(firstRow.querySelector('td:nth-child(3)').textContent, '50%');
    equal(firstRow.querySelector('td:nth-child(4)').innerHTML, '<div class="flaky-bar" style="width:250px"></div>');
});

test('handleValidHashParameter', 5, function() {
    var historyInstance = new history.History();

    ok(overview.handleValidHashParameter(historyInstance, 'flipCount', "5"))
    ok(overview.handleValidHashParameter(historyInstance, 'flipCount', 5))
    ok(!overview.handleValidHashParameter(historyInstance, 'flipCount', "notanumber"))
    ok(!overview.handleValidHashParameter(historyInstance, 'flipCount', "5notanumber"))
    ok(!overview.handleValidHashParameter(historyInstance, 'randomKey', "5"))
});

test('navbar', 3, function() {
    var flipCount = 5;
    var container = document.createElement('div');
    container.innerHTML = overview._htmlForNavBar(flipCount);

    ok(container.querySelector('select'));

    var sliderContainer = container.querySelector('#flip-slider-container');
    ok(sliderContainer);

    var range = sliderContainer.querySelector('input');
    equal(range.value, "5");
});
