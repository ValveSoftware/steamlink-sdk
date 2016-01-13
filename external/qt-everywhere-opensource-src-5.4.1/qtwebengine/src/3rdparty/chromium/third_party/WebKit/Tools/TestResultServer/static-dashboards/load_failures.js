// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var loadfailures = loadfailures || {};

(function() {

loadfailures._groupIndex = 0;
loadfailures._testTypeIndex = 0;
loadfailures._failureData = {};
loadfailures._loader = null;

// FIXME: This is a gross hack to make it so that changing the test type and group in loadNextTestType don't reload the page.
history.reloadRequiringParameters = history.reloadRequiringParameters.filter(function(item) { return item != 'testType' && item != 'group'; });

loadfailures.loadNextTestType = function(historyInstance)
{
    var group = builders.groups[loadfailures._groupIndex];

    if (loadfailures._loader) {
        var testType = builders.testTypes[loadfailures._testTypeIndex];
        var failures = loadfailures._loader.buildersThatFailedToLoad();
        if (failures.length)
            loadfailures._failureData[group].failingBuilders[testType] = failures;

        var stale = loadfailures._loader.staleBuilders();
        if (stale.length)
            loadfailures._failureData[group].staleBuilders[testType] = stale;

        if ((failures.length || stale.length) && !Object.keys(g_resultsByBuilder).length)
            loadfailures._failureData[group].testTypesWithNoSuccessfullLoads.push(testType);

        for (var builder in builders.builderToMaster)
            loadfailures._failureData[group].builderToMaster[builder] = builders.builderToMaster[builder];
    }

    loadfailures._testTypeIndex++;
    if (loadfailures._testTypeIndex == builders.testTypes.length) {

        loadfailures._groupIndex++;
        loadfailures._testTypeIndex = 0;

        if (loadfailures._groupIndex == builders.groups.length) {
            loadfailures._generatePage();
            return;
        }
    }

    var group = builders.groups[loadfailures._groupIndex];
    if (!loadfailures._failureData[group]) {
        loadfailures._failureData[group] = {
            failingBuilders: {},
            staleBuilders: {},
            testTypesWithNoSuccessfullLoads: [],
            builderToMaster: {},
        }
    }
    historyInstance.crossDashboardState.group = group;
    historyInstance.crossDashboardState.testType = builders.testTypes[loadfailures._testTypeIndex];

    var totalIterations = builders.groups.length * builders.testTypes.length;
    var currentIteration = loadfailures._groupIndex * builders.testTypes.length + loadfailures._testTypeIndex
    $('content').innerHTML = 'Loading ' + currentIteration + '/' + totalIterations + ' ' +
        historyInstance.crossDashboardState.group + ': ' + historyInstance.crossDashboardState.testType + '...';

    // FIXME: Gross hack to allow loading all the builders for different test types.
    // Change loader.js to allow you to pass in the state that it fills instead of setting globals.
    g_resultsByBuilder = {};
    loadfailures._loader = new loader.Loader()
    loadfailures._loader.load();
}

loadfailures._generatePage = function()
{
    $('content').innerHTML = loadfailures._html(loadfailures._failureData);
}

loadfailures._htmlForBuilder = function(builder, testType, builderToMaster)
{
    return '<tr class="builder">' +
        '<td>' + builder +
        '<td><a href="http://test-results.appspot.com/testfile?testtype=' +
            testType + '&builder=' + builder + '&master=' + builderToMaster[builder].name + '">uploaded results</a>' +
        '<td><a href="' + builderToMaster[builder].builderPath(builder) + '">buildbot</a>' +
    '</tr>';
}

loadfailures._html = function(failureData)
{
    var html = '';
    Object.keys(failureData).forEach(function(group) {
        var failingBuildersByTestType = failureData[group].failingBuilders;
        var staleBuildersByTestType = failureData[group].staleBuilders;
        var testTypesWithNoSuccessfullLoads = failureData[group].testTypesWithNoSuccessfullLoads;
        var builderToMaster = failureData[group].builderToMaster;

        var testTypes = testTypesWithNoSuccessfullLoads.concat(Object.keys(failingBuildersByTestType).concat(Object.keys(staleBuildersByTestType)));
        var uniqueTestTypes = testTypes.sort().filter(function(value, index, array) {
            return array.indexOf(value) === index;
        });

        if (!uniqueTestTypes.length)
            return;

        html += '<h1>' + group + '</h1>' +
            '<table><tr><th>Test type</th><th>>1 week stale</th><th>>1 day stale, <1 week stale</th></tr>';

        uniqueTestTypes.forEach(function(testType) {
            var failures = failingBuildersByTestType[testType] || [];
            var failureHtml = '';
            failures.sort().forEach(function(builder) {
                failureHtml += loadfailures._htmlForBuilder(builder, testType, builderToMaster);
            });

            var stale = staleBuildersByTestType[testType] || [];
            var staleHtml = '';
            stale.sort().forEach(function(builder) {
                staleHtml += loadfailures._htmlForBuilder(builder, testType, builderToMaster);
            });

            var noBuildersHtml = testTypesWithNoSuccessfullLoads.indexOf(testType) != -1 ? '<b>No builders with up to date results.</b>' : '';

            html += '<tr>' +
                '<td><a href="http://test-results.appspot.com/testfile?name=results.json&testtype=' + testType + '" target=_blank>' +
                    testType +
                '</a></td>' +
                '<td>' + noBuildersHtml + '<table>' + failureHtml + '</table></td>' +
                '<td><table>' + staleHtml + '</table></td>' +
            '</tr>';
        });
        html += '</table>';
    });
    return html;
}

// FIXME: Once dashboard_base, loader and ui stop using the g_history global, we can stop setting it here.
g_history = new history.History({
    generatePage: loadfailures.loadNextTestType,
});
g_history.parseCrossDashboardParameters();

window.addEventListener('load', function() {
    // FIXME: Come up with a better way to do this. This early return is just to avoid
    // executing this code when it's loaded in the unittests.
    if (!$('content'))
        return;
    loadfailures.loadNextTestType(g_history);
}, false);

})();
