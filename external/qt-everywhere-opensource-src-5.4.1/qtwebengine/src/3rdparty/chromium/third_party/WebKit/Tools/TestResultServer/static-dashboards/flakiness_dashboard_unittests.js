// Copyright (C) 2011 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

module('flakiness_dashboard');

// FIXME(jparent): Rename this once it isn't globals.
function resetGlobals()
{
    allExpectations = null;
    g_resultsByBuilder = {};
    g_allTestsTrie = null;
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    g_testToResultsMap = {};

    for (var key in history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES)
        historyInstance.crossDashboardState[key] = history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES[key];

    LOAD_BUILDBOT_DATA({
        "no_upload_test_types": [
            "webkit_unit_tests"
        ],
        'masters': [{
            name: 'ChromiumWebkit',
            url_name: "chromium.webkit",
            tests: {
                'layout-tests': {'builders': ['WebKit Linux', 'WebKit Linux (dbg)', 'WebKit Linux (deps)', 'WebKit Mac10.7', 'WebKit Win', 'WebKit Win (dbg)']},
                'unit_tests': {'builders': ['Linux Tests']},
            },
            groups: ['@ToT Chromium', '@ToT Blink'],
        },{
            name :'ChromiumWin',
            url_name: "chromium.win",
            tests: {
                'ash_unittests': {'builders': ['XP Tests (1)', 'Win7 Tests (1)']},
                'unit_tests': {'builders': ['Linux Tests']},
            },
            groups: ['@ToT Chromium'],
        }],
    });

   return historyInstance;
}

var FAILURE_MAP = {"A": "AUDIO", "C": "CRASH", "F": "TEXT", "I": "IMAGE", "O": "MISSING",
    "N": "NO DATA", "P": "PASS", "T": "TIMEOUT", "Y": "NOTRUN", "X": "SKIP", "Z": "IMAGE+TEXT"}

test('splitTestList', 1, function() {
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    historyInstance.dashboardSpecificState.tests = 'test.foo test.foo1\ntest.foo2\ntest.foo3,foo\\bar\\baz.html';
    equal(splitTestList().toString(), 'test.foo,test.foo1,test.foo2,test.foo3,foo/bar/baz.html');
});

test('headerForTestTableHtml', 1, function() {
    var container = document.createElement('div');
    container.innerHTML = headerForTestTableHtml();
    equal(container.querySelectorAll('input').length, 4);
});

test('htmlForTestTypeSwitcherGroup', 6, function() {
    resetGlobals();
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    var container = document.createElement('div');
    historyInstance.crossDashboardState.testType = 'ash_unittests';
    container.innerHTML = ui.html.testTypeSwitcher(true);
    var selects = container.querySelectorAll('select');
    equal(selects.length, 2);
    var group = selects[1];
    equal(group.parentNode.textContent.indexOf('Group:'), 0);
    equal(group.children.length, 1);

    historyInstance.crossDashboardState.testType = 'layout-tests';
    container.innerHTML = ui.html.testTypeSwitcher(true);
    var selects = container.querySelectorAll('select');
    equal(selects.length, 2);
    var group = selects[1];
    equal(group.parentNode.textContent.indexOf('Group:'), 0);
    equal(group.children.length, 2);
});

test('htmlForIndividualTestOnAllBuilders', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT Blink', 'layout-tests');
    equal(htmlForIndividualTestOnAllBuilders('foo/nonexistant.html'), '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all recorded runs.</div>');
});

test('htmlForIndividualTestOnAllBuildersWithResultsLinksNonexistant', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT Blink', 'layout-tests');
    equal(htmlForIndividualTestOnAllBuildersWithResultsLinks('foo/nonexistant.html'),
        '<div class="not-found">Test not found. Either it does not exist, is skipped or passes on all recorded runs.</div>' +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div>' +
                '<span class=link onclick="g_history.setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
                '<span class=link onclick="g_history.setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
                '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b>' +
            '</div>' +
        '</div>');
});

test('htmlForIndividualTestOnAllBuildersWithResultsLinks', 1, function() {
    resetGlobals();
    builders.loadBuildersList('@ToT Blink', 'layout-tests');

    var builderName = 'WebKit Linux';
    g_resultsByBuilder[builderName] = {buildNumbers: [2, 1], blinkRevision: [1234, 1233], failure_map: FAILURE_MAP};

    var test = 'dummytest.html';
    var resultsObject = createResultsObjectForTest(test, builderName);
    resultsObject.rawResults = [[1, 'F']];
    resultsObject.rawTimes = [[1, 0]];
    resultsObject.bugs = ["crbug.com/1234", "webkit.org/5678"];
    g_testToResultsMap[test] = [resultsObject];

    equal(htmlForIndividualTestOnAllBuildersWithResultsLinks(test),
        '<table class=test-table><thead><tr>' +
                '<th sortValue=test><div class=table-header-content><span></span><span class=header-text>test</span></div></th>' +
                '<th sortValue=bugs><div class=table-header-content><span></span><span class=header-text>bugs</span></div></th>' +
                '<th sortValue=expectations><div class=table-header-content><span></span><span class=header-text>expectations</span></div></th>' +
                '<th sortValue=slowest><div class=table-header-content><span></span><span class=header-text>slowest run</span></div></th>' +
                '<th sortValue=flakiness colspan=10000><div class=table-header-content><span></span><span class=header-text>flakiness (numbers are runtimes in seconds)</span></div></th>' +
            '</tr></thead>' +
            '<tbody><tr>' +
                '<td class="test-link builder-name">WebKit Linux' +
                '<td class=options-container>' +
                    '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div>' +
                    '<div><a href="http://webkit.org/5678">webkit.org/5678</a></div>' +
                '<td class=options-container><td><td title="TEXT. Click for more info." class="results TEXT" onclick=\'showPopupForBuild(event, "WebKit Linux",0,"dummytest.html")\'>&nbsp;' +
                '<td title="NO DATA. Click for more info." class="results NODATA" onclick=\'showPopupForBuild(event, "WebKit Linux",1,"dummytest.html")\'>&nbsp;' +
            '</tbody>' +
        '</table>' +
        '<div>The following builders either don\'t run this test (e.g. it\'s skipped) or all recorded runs passed:</div>' +
        '<div class=skipped-builder-list>' +
            '<div class=skipped-builder>WebKit Linux (dbg)</div><div class=skipped-builder>WebKit Mac10.7</div><div class=skipped-builder>WebKit Win</div><div class=skipped-builder>WebKit Win (dbg)</div>' +
        '</div>' +
        '<div class=expectations test=dummytest.html>' +
            '<div><span class=link onclick="g_history.setQueryParameter(\'showExpectations\', true)">Show results</span> | ' +
            '<span class=link onclick="g_history.setQueryParameter(\'showLargeExpectations\', true)">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');
});

test('individualTestsForSubstringList', 2, function() {
    var builderName = 'WebKit Linux';
    g_resultsByBuilder[builderName] = {
        buildNumbers: [2, 1],
        blinkRevision: [1234, 1233],
        failure_map: FAILURE_MAP,
        tests: {
            'foo/one.html': { results: [1, 'F'], times: [1, 0] },
            'virtual/foo/one.html': { results: [1, 'F'], times: [1, 0] },
        }
    };

    g_history.dashboardSpecificState.showChrome = true;
    var testToMatch = 'foo/one.html';
    g_history.dashboardSpecificState.tests = testToMatch;
    deepEqual(individualTestsForSubstringList(), [testToMatch, 'virtual/foo/one.html']);

    g_history.dashboardSpecificState.showChrome = false;
    deepEqual(individualTestsForSubstringList(), [testToMatch]);
});

test('htmlForIndividualTest', 2, function() {
    var historyInstance = resetGlobals();
    builders.loadBuildersList('@ToT Blink', 'layout-tests');
    var test = 'foo/nonexistant.html';

    historyInstance.dashboardSpecificState.showChrome = false;

    equal(htmlForIndividualTest(test), htmlForIndividualTestOnAllBuilders(test) +
        '<div class=expectations test=foo/nonexistant.html>' +
            '<div><span class=link onclick=\"g_history.setQueryParameter(\'showExpectations\', true)\">Show results</span> | ' +
            '<span class=link onclick=\"g_history.setQueryParameter(\'showLargeExpectations\', true)\">Show large thumbnails</span> | ' +
            '<b>Only shows actual results/diffs from the most recent *failure* on each bot.</b></div>' +
        '</div>');

    historyInstance.dashboardSpecificState.showChrome = true;

    equal(htmlForIndividualTest(test),
        '<h2><a href="' + TEST_URL_BASE_PATH_FOR_BROWSING + 'foo/nonexistant.html" target="_blank">foo/nonexistant.html</a></h2>' +
        htmlForIndividualTestOnAllBuildersWithResultsLinks(test));
});

test('linkifyBugs', 4, function() {
    equal(linkifyBugs(["crbug.com/1234", "webkit.org/5678"]),
        '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div><div><a href="http://webkit.org/5678">webkit.org/5678</a></div>');
    equal(linkifyBugs(["crbug.com/1234"]), '<div><a href="http://crbug.com/1234">crbug.com/1234</a></div>');
    equal(linkifyBugs(["Bug(nick)"]), '<div>Bug(nick)</div>');
    equal(linkifyBugs([]), '');
});

test('htmlForSingleTestRow', 1, function() {
    var historyInstance = resetGlobals();
    var builder = 'dummyBuilder';
    var test = createResultsObjectForTest('foo/exists.html', builder);
    historyInstance.dashboardSpecificState.showNonFlaky = true;
    g_resultsByBuilder[builder] = {buildNumbers: [2, 1], blinkRevision: [1234, 1233], failure_map: FAILURE_MAP};
    test.rawResults = [[1, 'F'], [2, 'I']];
    test.rawTimes = [[1, 0], [2, 5]];
    var expected = '<tr>' +
        '<td class="test-link"><span class="link" onclick="g_history.setQueryParameter(\'tests\',\'foo/exists.html\');">foo/exists.html</span>' +
        '<td class=options-container><a href="https://code.google.com/p/chromium/issues/entry?template=Layout%20Test%20Failure&summary=Layout%20Test%20foo%2Fexists.html%20is%20failing&comment=The%20following%20layout%20test%20is%20failing%20on%20%5Binsert%20platform%5D%0A%0Afoo%2Fexists.html%0A%0AProbable%20cause%3A%0A%0A%5Binsert%20probable%20cause%5D">File new bug</a>' +
        '<td class=options-container>' +
        '<td>' +
        '<td title="TEXT. Click for more info." class="results TEXT" onclick=\'showPopupForBuild(event, "dummyBuilder",0,"foo/exists.html")\'>&nbsp;' +
        '<td title="IMAGE. Click for more info." class="results IMAGE" onclick=\'showPopupForBuild(event, "dummyBuilder",1,"foo/exists.html")\'>5';
    equal(htmlForSingleTestRow(test), expected);
});

test('lookupVirtualTestSuite', 2, function() {
    equal(lookupVirtualTestSuite('fast/canvas/foo.html'), '');
    equal(lookupVirtualTestSuite('virtual/gpu/fast/canvas/foo.html'), 'virtual/gpu/fast/canvas');
});

test('baseTest', 2, function() {
    equal(baseTest('fast/canvas/foo.html', ''), 'fast/canvas/foo.html');
    equal(baseTest('virtual/gpu/fast/canvas/foo.html', 'virtual/gpu/fast/canvas'), 'fast/canvas/foo.html');
});

test('sortTests', 4, function() {
    var test1 = createResultsObjectForTest('foo/test1.html', 'dummyBuilder');
    var test2 = createResultsObjectForTest('foo/test2.html', 'dummyBuilder');
    var test3 = createResultsObjectForTest('foo/test3.html', 'dummyBuilder');
    test1.expectations = 'b';
    test2.expectations = 'a';
    test3.expectations = '';

    var tests = [test1, test2, test3];
    sortTests(tests, 'expectations', FORWARD);
    deepEqual(tests, [test2, test1, test3]);
    sortTests(tests, 'expectations', BACKWARD);
    deepEqual(tests, [test3, test1, test2]);

    test1.bugs = 'b';
    test2.bugs = 'a';
    test3.bugs = '';

    var tests = [test1, test2, test3];
    sortTests(tests, 'bugs', FORWARD);
    deepEqual(tests, [test2, test1, test3]);
    sortTests(tests, 'bugs', BACKWARD);
    deepEqual(tests, [test3, test1, test2]);
});

test('popup', 2, function() {
    ui.popup.show(document.body, 'dummy content');
    ok(document.querySelector('#popup'));
    ui.popup.hide();
    ok(!document.querySelector('#popup'));
});

test('gpuResultsPath', 3, function() {
  equal(gpuResultsPath('777777', 'Win7 Release (ATI)'), '777777_Win7_Release_ATI_');
  equal(gpuResultsPath('123', 'GPU Linux (dbg)(NVIDIA)'), '123_GPU_Linux_dbg_NVIDIA_');
  equal(gpuResultsPath('12345', 'GPU Mac'), '12345_GPU_Mac');
});

test('TestTrie', 3, function() {
    var builders = {
        "Dummy Chromium Windows Builder": true,
        "Dummy GTK Linux Builder": true,
        "Dummy Apple Mac Lion Builder": true
    };

    var resultsByBuilder = {
        "Dummy Chromium Windows Builder": {
            tests: {
                "foo": true,
                "foo/bar/1.html": true,
                "foo/bar/baz": true
            }
        },
        "Dummy GTK Linux Builder": {
            tests: {
                "bar": true,
                "foo/1.html": true,
                "foo/bar/2.html": true,
                "foo/bar/baz/1.html": true,
            }
        },
        "Dummy Apple Mac Lion Builder": {
            tests: {
                "foo/bar/3.html": true,
                "foo/bar/baz/foo": true,
            }
        }
    };
    var expectedTrie = {
        "foo": {
            "bar": {
                "1.html": true,
                "2.html": true,
                "3.html": true,
                "baz": {
                    "1.html": true,
                    "foo": true
                }
            },
            "1.html": true
        },
        "bar": true
    }

    var trie = new TestTrie(builders, resultsByBuilder);
    deepEqual(trie._trie, expectedTrie);

    var leafsOfCompleteTrieTraversal = [];
    var expectedLeafs = ["foo/bar/1.html", "foo/bar/baz/1.html", "foo/bar/baz/foo", "foo/bar/2.html", "foo/bar/3.html", "foo/1.html", "bar"];
    trie.forEach(function(triePath) {
        leafsOfCompleteTrieTraversal.push(triePath);
    });
    deepEqual(leafsOfCompleteTrieTraversal, expectedLeafs);

    var leafsOfPartialTrieTraversal = [];
    expectedLeafs = ["foo/bar/1.html", "foo/bar/baz/1.html", "foo/bar/baz/foo", "foo/bar/2.html", "foo/bar/3.html"];
    trie.forEach(function(triePath) {
        leafsOfPartialTrieTraversal.push(triePath);
    }, "foo/bar");
    deepEqual(leafsOfPartialTrieTraversal, expectedLeafs);
});

test('changeTestTypeInvalidatesGroup', 1, function() {
    var historyInstance = resetGlobals();
    var originalGroup = '@ToT Blink';
    var originalTestType = 'layout-tests';
    builders.loadBuildersList(originalGroup, originalTestType);
    historyInstance.crossDashboardState.group = originalGroup;
    historyInstance.crossDashboardState.testType = originalTestType;

    historyInstance.invalidateQueryParameters({'testType': 'ui_tests'});
    notEqual(historyInstance.crossDashboardState.group, originalGroup, "group should have been invalidated");
});

test('shouldShowTest', 9, function() {
    var historyInstance = new history.History(flakinessConfig);
    historyInstance.parseParameters();
    // FIXME(jparent): Change to use the flakiness_dashboard's history object
    // once it exists, rather than tracking global.
    g_history = historyInstance;
    var test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');

    equal(shouldShowTest(test), false, 'default layout test, hide it.');
    historyInstance.dashboardSpecificState.showNonFlaky = true;
    equal(shouldShowTest(test), true, 'show correct expectations.');
    historyInstance.dashboardSpecificState.showNonFlaky = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.expectations = "WONTFIX";
    equal(shouldShowTest(test), false, 'by default hide wontfix');
    historyInstance.dashboardSpecificState.showWontFix = true;
    equal(shouldShowTest(test), true, 'show wontfix');
    historyInstance.dashboardSpecificState.showWontFix = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.expectations = "SKIP";
    equal(shouldShowTest(test), false, 'we hide skip tests by default');
    historyInstance.dashboardSpecificState.showSkip = true;
    equal(shouldShowTest(test), true, 'show skip test');
    historyInstance.dashboardSpecificState.showSkip = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    test.isFlaky = true;
    equal(shouldShowTest(test), false, 'hide flaky tests by default');
    historyInstance.dashboardSpecificState.showFlaky = true;
    equal(shouldShowTest(test), true, 'show flaky test');
    historyInstance.dashboardSpecificState.showFlaky = false;

    test = createResultsObjectForTest('foo/test.html', 'dummyBuilder');
    historyInstance.crossDashboardState.testType = 'not layout tests';
    equal(shouldShowTest(test), true, 'show all non layout tests');
});
