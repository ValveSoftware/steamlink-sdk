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

var overview = overview || {};

(function() {

overview._resultsByTestType = {};
overview._testTypeIndex = 0;

// FIXME: This is a gross hack to make it so that changing the test type in loadNextTestType doesn't reload the page.
history.reloadRequiringParameters = history.reloadRequiringParameters.filter(function(item) { return item != 'testType'; });

overview.loadNextTestType = function(historyInstance)
{
    if (overview._testTypeIndex == builders.testTypes.length) {
        overview._generatePage();
        return;
    }

    historyInstance.crossDashboardState.testType = builders.testTypes[overview._testTypeIndex++];

    $('content').innerHTML = (overview._testTypeIndex - 1) + '/' + builders.testTypes.length + ' loaded. Loading ' + historyInstance.crossDashboardState.testType + '...';

    // FIXME: Gross hack to allow loading all the builders for different test types.
    // Change loader.js to allow you to pass in the state that it fills instead of setting globals.
    g_resultsByBuilder = {};
    overview._resultsByTestType[historyInstance.crossDashboardState.testType] = g_resultsByBuilder;
    new loader.Loader().load();
}

overview._getFlakyData = function(allTestTypes, resultsByTestType, flipCountThreshold)
{
    var flakyData = {};
    allTestTypes.forEach(function(testType) {
        flakyData[testType] = {
            flakyBelowThreshold: {},
            flaky: {},
            testCount: 0
        }

        var resultsByBuilder = resultsByTestType[testType];
        for (var builder in resultsByBuilder) {
            var totalTestCount = results.testCounts(resultsByBuilder[builder][results.NUM_FAILURES_BY_TYPE]).totalTests[0];
            flakyData[testType].testCount = Math.max(totalTestCount, flakyData[testType].testCount);

            var allTestsForThisBuilder = resultsByBuilder[builder].tests;
            for (var test in allTestsForThisBuilder) {
                var resultsForTest = {};
                var testData = resultsByBuilder[builder].tests[test].results;
                var failureMap = resultsByBuilder[builder][results.FAILURE_MAP];
                results.determineFlakiness(failureMap, testData, resultsForTest);

                if (resultsForTest.isFlaky)
                    flakyData[testType].flaky[test] = true;

                if (!resultsForTest.isFlaky || resultsForTest.flipCount <= flipCountThreshold)
                    continue;
                flakyData[testType].flakyBelowThreshold[test] = true;
            }
        }
    });
    return flakyData;
}

overview._generatePage = function()
{
    var flipCountThreshold = Number(g_history.dashboardSpecificState.flipCount);
    var flakyData = overview._getFlakyData(builders.testTypes, overview._resultsByTestType, flipCountThreshold);
    $('content').innerHTML = overview._htmlForFlakyTests(flakyData, g_history.crossDashboardState.group) +
        '<div>*Tests that fail due to a bad patch being committed are counted as flaky.</div>';
}

overview._htmlForFlakyTests = function(flakyData, group)
{
    var html = '<table><tr><th>Test type</th><th>flaky count / total count</th><th>percent</th><th></th></tr>';

    Object.keys(flakyData).forEach(function(testType) {
        var testCount = flakyData[testType].testCount;
        if (!testCount)
            return;

        // We want the list of tests to stay stable as you drag the flakiness slider, so only
        // exclude tests that never flake, even at the lowest flakiness threshold.
        var flakeCountIgnoringThreshold = Object.keys(flakyData[testType].flaky).length;
        if (!g_history.dashboardSpecificState.showNoFlakes && !flakeCountIgnoringThreshold)
            return;

        var tests = Object.keys(flakyData[testType].flakyBelowThreshold);
        var flakyCount = tests.length;
        var percentage = Math.round(100 * flakyCount / testCount);
        html += '<tr>' +
            '<td><a href="flakiness_dashboard.html#group=' + group + '&testType=' + testType + '&tests=' + tests.join(',') + '" target=_blank>' +
                testType +
            '</a></td>' +
            '<td>' + flakyCount + ' / ' + testCount + '</td>' +
            '<td>' + percentage + '%</td>' +
            '<td><div class="flaky-bar" style="width:' + percentage * 5 + 'px"></div>'
        '</tr>';
    });

    return html + '</table>';
}

overview.handleValidHashParameter = function(historyInstance, key, value) {
    switch(key) {
    case 'flipCount':
        return history.validateParameter(historyInstance.dashboardSpecificState, key, value,
            function() {
                return !isNaN(Number(value));
            });

    case 'showNoFlakes':
        historyInstance.dashboardSpecificState[key] = value == 'true';
        return true;

    default:
        return false;
    }
}

overview._htmlForNavBar = function(flipCount, showNoFlakes)
{
    return ui.html.navbar(ui.html.select('Group', 'group', builders.getAllGroupNames())) +
        '<div id=flip-slider-container>' +
            ui.html.range('flipCount', 'Flakiness threshold (low-->high):', 1, 50, flipCount) +
            ui.html.checkbox('showNoFlakes', 'Show test suites with no flakes', showNoFlakes) +
        '</div>';
}

// FIXME: Once dashboard_base, loader and ui stop using the g_history global, we can stop setting it here.
g_history = new history.History({
    defaultStateValues: {
        flipCount: 1,
        showNoFlakes: false
    },
    generatePage: overview.loadNextTestType,
    handleValidHashParameter: overview.handleValidHashParameter,
});
g_history.parseCrossDashboardParameters();

window.addEventListener('load', function() {
    // FIXME: Come up with a better way to do this. This early return is just to avoid
    // executing this code when it's loaded in the unittests.
    if (!$('navbar'))
        return;

    // Need to parseParameters so that flipCount has the correct value.
    g_history.parseParameters();
    $('navbar').innerHTML = overview._htmlForNavBar(g_history.dashboardSpecificState.flipCount);
    overview.loadNextTestType(g_history);
}, false);

})();
