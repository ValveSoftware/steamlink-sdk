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

var unittest = unittest || {};

(function () {

module("results");

var MockResultsBaseURL = 'https://storage.googleapis.com/chromium-layout-test-archives/Mock_Builder/results/layout-test-results';

unittest.kExampleResultsJSON = {
    "tests": {
        "scrollbars": {
            "custom-scrollbar-with-incomplete-style.html": {
                "expected": "IMAGE",
                "actual": "IMAGE",
            },
            "expected-wontfix": {
                "expected": "WONTFIX",
                "actual": "SKIP",
            },
            "flaky-scrollbar.html": {
                "expected": "PASS",
                "actual": "PASS TEXT",
                "is_unexpected": true,
            },
            "unexpected-failing-flaky-scrollbar.html": {
                "expected": "TEXT",
                "actual": "TIMEOUT TEXT",
                "is_unexpected": true,
            },
            "unexpected-pass.html": {
                "expected": "FAIL",
                "actual": "PASS",
                "is_unexpected": true,
            }
        },
        "userscripts": {
            "user-script-video-document.html": {
                "expected": "FAIL",
                "actual": "TEXT",
            },
            "another-test.html": {
                "expected": "PASS",
                "actual": "TEXT",
                "is_unexpected": true,
            }
        },
    },
    "skipped": 339,
    "num_regressions": 14,
    "interrupted": false,
    "layout_tests_dir": "\/mnt\/data\/b\/build\/slave\/Webkit_Linux\/build\/src\/third_party\/WebKit\/LayoutTests",
    "version": 3,
    "num_passes": 15566,
    "has_pretty_patch": false,
    "fixable": 1233,
    "num_flaky":1,
    "has_wdiff": true,
    "blink_revision": "90430"
};

test("ResultAnalyzer", 44, function() {
    var analyzer;

    analyzer = new results.ResultAnalyzer({expected: 'PASS', actual: 'TEXT', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['TEXT']);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'PASS TIMEOUT', actual: 'TEXT', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['TEXT']);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'TEXT', actual: 'TEXT TIMEOUT', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['TIMEOUT']);
    ok(!analyzer.succeeded());
    ok(analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'PASS', actual: 'TEXT TIMEOUT', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['TEXT', 'TIMEOUT']);
    ok(!analyzer.succeeded());
    ok(analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'PASS TIMEOUT', actual: 'PASS TIMEOUT'});
    ok(!analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), []);
    ok(analyzer.succeeded());
    ok(analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'PASS TIMEOUT', actual: 'TIMEOUT PASS'});
    ok(!analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), []);
    ok(analyzer.succeeded());
    ok(analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'FAIL', actual: 'TIMEOUT', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['TIMEOUT']);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'FAIL', actual: 'IMAGE', is_unexpected: true});
    ok(analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), ['IMAGE']);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'FAIL', actual: 'AUDIO'});
    ok(!analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), []);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'FAIL', actual: 'TEXT'});
    ok(!analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), []);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());

    analyzer = new results.ResultAnalyzer({expected: 'FAIL', actual: 'IMAGE+TEXT'});
    ok(!analyzer.hasUnexpectedFailures());
    deepEqual(analyzer.unexpectedResults(), []);
    ok(!analyzer.succeeded());
    ok(!analyzer.flaky());
});

test("expectedFailures", 1, function() {
    var expectedFailures = results.expectedFailures(unittest.kExampleResultsJSON);
    deepEqual(expectedFailures, {
        "scrollbars/custom-scrollbar-with-incomplete-style.html": {
            "expected": "IMAGE",
            "actual": "IMAGE"
        },
        "userscripts/user-script-video-document.html": {
            "expected": "FAIL",
            "actual": "TEXT"
        }
    });
});

test("unexpectedFailures", 1, function() {
    var unexpectedFailures = results.unexpectedFailures(unittest.kExampleResultsJSON);
    deepEqual(unexpectedFailures, {
        "userscripts/another-test.html": {
            "expected": "PASS",
            "actual": "TEXT",
            "is_unexpected": true,
        }
    });
});

test("unexpectedFailuresByTest", 1, function() {
    var unexpectedFailuresByTest = results.unexpectedFailuresByTest({
        "Mock Builder": unittest.kExampleResultsJSON
    });
    deepEqual(unexpectedFailuresByTest, {
        "userscripts/another-test.html": {
            "Mock Builder": {
                "expected": "PASS",
                "actual": "TEXT",
                "is_unexpected": true,
            }
        }
    });
});

test("failureInfoForTestAndBuilder", 1, function() {
    var unexpectedFailuresByTest = results.unexpectedFailuresByTest({
        "Mock Builder": unittest.kExampleResultsJSON
    });
    var failureInfo = results.failureInfoForTestAndBuilder(unexpectedFailuresByTest, "userscripts/another-test.html", "Mock Builder");
    deepEqual(failureInfo, {
        "testName": "userscripts/another-test.html",
        "builderName": "Mock Builder",
        "failureTypeList": ["TEXT"],
    });
});

test("resultKind", 12, function() {
    equals(results.resultKind("http://example.com/foo-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo.bar-actual.txt"), "actual");
    equals(results.resultKind("http://example.com/foo.bar-expected.txt"), "expected");
    equals(results.resultKind("http://example.com/foo.bar-diff.txt"), "diff");
    equals(results.resultKind("http://example.com/foo-actual.png"), "actual");
    equals(results.resultKind("http://example.com/foo-expected.png"), "expected");
    equals(results.resultKind("http://example.com/foo-diff.png"), "diff");
    equals(results.resultKind("http://example.com/foo-pretty-diff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-wdiff.html"), "diff");
    equals(results.resultKind("http://example.com/foo-xyz.html"), "unknown");
});

test("resultType", 12, function() {
    equals(results.resultType("http://example.com/foo-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-actual.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-expected.txt"), "text");
    equals(results.resultType("http://example.com/foo.bar-diff.txt"), "text");
    equals(results.resultType("http://example.com/foo-actual.png"), "image");
    equals(results.resultType("http://example.com/foo-expected.png"), "image");
    equals(results.resultType("http://example.com/foo-diff.png"), "image");
    equals(results.resultType("http://example.com/foo-pretty-diff.html"), "text");
    equals(results.resultType("http://example.com/foo-wdiff.html"), "text");
    equals(results.resultType("http://example.com/foo.xyz"), "text");
});

test("resultNodeForTest", 4, function() {
    deepEqual(results.resultNodeForTest(unittest.kExampleResultsJSON, "userscripts/another-test.html"), {
        "expected": "PASS",
        "actual": "TEXT",
        "is_unexpected": true,
    });
    equals(results.resultNodeForTest(unittest.kExampleResultsJSON, "foo.html"), null);
    equals(results.resultNodeForTest(unittest.kExampleResultsJSON, "userscripts/foo.html"), null);
    equals(results.resultNodeForTest(unittest.kExampleResultsJSON, "userscripts/foo/bar.html"), null);
});

asyncTest("walkHistory", 1, function() {
    var simulator = new NetworkSimulator();

    var keyMap = {
        "Mock_Builder": {
            "11108": {
                "tests": {
                    "userscripts": {
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "TEXT",
                            "is_unexpected": true,
                        }
                    },
                },
                "blink_revision": "90430"
            },
            "11107":{
                "tests": {
                    "userscripts": {
                        "user-script-video-document.html": {
                            "expected": "FAIL",
                            "actual": "TEXT",
                            "is_unexpected": false,
                        },
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "TEXT",
                            "is_unexpected": true,
                        }
                    },
                },
                "blink_revision": "90429"
            },
            "11106":{
                "tests": {
                    "userscripts": {
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "TEXT",
                            "is_unexpected": true,
                        }
                    },
                },
                "blink_revision": "90426"
            },
            "11105":{
                "tests": {
                    "userscripts": {
                        "user-script-video-document.html": {
                            "expected": "FAIL",
                            "actual": "TEXT",
                            "is_unexpected": false,
                        },
                    },
                },
                "blink_revision": "90424"
            },
        },
        "Another_Builder": {
            "22202":{
                "tests": {
                    "userscripts": {
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "TEXT",
                            "is_unexpected": true,
                        }
                    },
                },
                "blink_revision": "90426"
            },
            "22201":{
                "tests": {
                },
                "blink_revision": "90425"
            },
        },
    };

    simulator.jsonp = function(url) {
        var result = keyMap[/[^/]+_Builder/.exec(url)][/\d+/.exec(url)];
        return Promise.resolve(result ? result : {});
    };

    simulator.json = function(url) {
        if (/Mock Builder/.test(url))
            return Promise.resolve({cachedBuilds: [11101, 11102, 11103, 11104, 11105, 11106, 11107, 11108]});
        else if (/Another Builder/.test(url))
            return Promise.resolve({cachedBuilds: [22201, 22202]});
        else
            return Promise.reject(false, 'Unexpected URL: ' + url);
    };

    simulator.runTest(function() {
            results.regressionRangeForFailure("Mock Builder", "userscripts/another-test.html")
                .then(function(result) {
                    var oldestFailingRevision = result[0];
                    var newestPassingRevision = result[1];
                    equals(oldestFailingRevision, 90426);
                    equals(newestPassingRevision, 90424);
                });
            results.unifyRegressionRanges(["Mock Builder", "Another Builder"], "userscripts/another-test.html")
                .then(function(result) {
                    var oldestFailingRevision = result[0];
                    var newestPassingRevision = result[1];
                    equals(oldestFailingRevision, 90426);
                    equals(newestPassingRevision, 90425);
                });
    }).then(start);
});

asyncTest("walkHistory (no revision)", 3, function() {
    var simulator = new NetworkSimulator();

    var keyMap = {
        "Mock_Builder": {
            "11103": {
                "tests": {
                    "userscripts": {
                        "another-test.html": {
                            "expected": "PASS",
                            "actual": "TEXT"
                        }
                    },
                },
                "blink_revision": ""
            },
            "11102":{
                "tests": {
                },
                "blink_revision": ""
            },
        },
    };

    simulator.jsonp = function(url) {
        var result = keyMap[/[^/]+_Builder/.exec(url)][/\d+/.exec(url)];
        return Promise.resolve(result ? result : {});
    };

    simulator.json = function(url) {
        return Promise.resolve({});
    };

    simulator.runTest(function() {
        results.regressionRangeForFailure("Mock Builder", "userscripts/another-test.html").then(function(result) {
            var oldestFailingRevision = result[0];
            var newestPassingRevision = result[1];
            equals(oldestFailingRevision, 0);
            equals(newestPassingRevision, 0);
        }).then(start);
    });
});

test("collectUnexpectedResults", 1, function() {
    var dictionaryOfResultNodes = {
        "foo": {
            "expected": "IMAGE",
            "actual": "IMAGE"
        },
        "bar": {
            "expected": "PASS",
            "actual": "PASS TEXT"
        },
        "baz": {
            "expected": "TEXT",
            "actual": "IMAGE"
        },
        "qux": {
            "expected": "PASS",
            "actual": "TEXT"
        },
        "taco": {
            "expected": "PASS",
            "actual": "TEXT"
        },
    };

    var collectedResults = results.collectUnexpectedResults(dictionaryOfResultNodes);
    deepEqual(collectedResults, ["TEXT", "IMAGE"]);
});

test("failureTypeToExtensionList", 5, function() {
    deepEqual(results.failureTypeToExtensionList('TEXT'), ['txt']);
    deepEqual(results.failureTypeToExtensionList('IMAGE+TEXT'), ['txt', 'png']);
    deepEqual(results.failureTypeToExtensionList('IMAGE'), ['png']);
    deepEqual(results.failureTypeToExtensionList('CRASH'), []);
    deepEqual(results.failureTypeToExtensionList('TIMEOUT'), []);
});

asyncTest("fetchResultsURLs", 5, function() {
    var simulator = new NetworkSimulator();

    var probedURLs = [];
    simulator.probe = function(url)
    {
        probedURLs.push(url);
        if (base.endsWith(url, '.txt'))
            return Promise.resolve();
        else if (/taco.+png$/.test(url))
            return Promise.resolve();
        else
            return Promise.reject();
    };

    simulator.runTest(function() {
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/another-test.html",
            'failureTypeList': ['IMAGE', 'CRASH'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, [
                MockResultsBaseURL + "/userscripts/another-test-crash-log.txt"
            ]);
        });
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/another-test.html",
            'failureTypeList': ['TIMEOUT'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, []);
        });
        results.fetchResultsURLs({
            'builderName': "Mock Builder",
            'testName': "userscripts/taco.html",
            'failureTypeList': ['IMAGE', 'IMAGE+TEXT'],
        }).then(function(resultURLs) {
            deepEqual(resultURLs, [
                MockResultsBaseURL + "/userscripts/taco-expected.png",
                MockResultsBaseURL + "/userscripts/taco-actual.png",
                MockResultsBaseURL + "/userscripts/taco-diff.png",
                MockResultsBaseURL + "/userscripts/taco-expected.txt",
                MockResultsBaseURL + "/userscripts/taco-actual.txt",
                MockResultsBaseURL + "/userscripts/taco-diff.txt",
            ]);
        });
    }).then(function() {
        deepEqual(probedURLs, [
            MockResultsBaseURL + "/userscripts/another-test-expected.png",
            MockResultsBaseURL + "/userscripts/another-test-actual.png",
            MockResultsBaseURL + "/userscripts/another-test-diff.png",
            MockResultsBaseURL + "/userscripts/another-test-crash-log.txt",
            MockResultsBaseURL + "/userscripts/taco-expected.png",
            MockResultsBaseURL + "/userscripts/taco-actual.png",
            MockResultsBaseURL + "/userscripts/taco-diff.png",
            MockResultsBaseURL + "/userscripts/taco-actual.txt",
            MockResultsBaseURL + "/userscripts/taco-expected.txt",
            MockResultsBaseURL + "/userscripts/taco-diff.txt",
        ]);
        start();
    });
});

asyncTest("fetchResultsByBuilder", 3, function() {
    var simulator = new NetworkSimulator();

    var probedURLs = [];
    simulator.jsonp = function(url)
    {
        probedURLs.push(url);
        return Promise.resolve(base.endsWith(url, 'results/layout-test-results/failing_results.json'));
    };

    simulator.runTest(function() {
        results.fetchResultsByBuilder(['MockBuilder1', 'MockBuilder2']).then(function(resultsByBuilder) {
            deepEqual(resultsByBuilder, {
                "MockBuilder1": true,
                "MockBuilder2": true,
            });
        });
    }).then(start);

    deepEqual(probedURLs, [
        MockResultsBaseURL.replace('Mock_Builder', 'MockBuilder1') + "/failing_results.json",
        MockResultsBaseURL.replace('Mock_Builder', 'MockBuilder2') + "/failing_results.json"
    ]);

});

})();
