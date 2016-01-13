// Copyright (C) 2013 Google Inc. All rights reserved.
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
//
// @fileoverview Creates a dashboard for tracking number of passes/failures per run.
//
// Currently, only webkit tests are supported, but adding other test types
// should just require the following steps:
//     -generate results.json for these tests
//     -copy them to the appropriate location
//     -add the builder name to the list of builders in dashboard_base.js.

function generatePage(historyInstance)
{
    var html = ui.html.testTypeSwitcher(true);
    html += '<div>' +
        ui.html.checkbox('rawValues', 'Show raw values', g_history.dashboardSpecificState.rawValues) +
        ui.html.checkbox('showOutliers', 'Show outliers', g_history.dashboardSpecificState.showOutliers) +
    '</div>';
    for (var builder in currentBuilders())
        html += htmlForBuilder(builder);
    document.body.innerHTML = html;
}

function handleValidHashParameter(historyInstance, key, value)
{
    switch(key) {
    case 'rawValues':
    case 'showOutliers':
        historyInstance.dashboardSpecificState[key] = value == 'true';
        return true;

    default:
        return false;
    }
}

var defaultDashboardSpecificStateValues = {
    rawValues: false,
    showOutliers: true
};

var aggregateResultsConfig = {
    defaultStateValues: defaultDashboardSpecificStateValues,
    generatePage: generatePage,
    handleValidHashParameter: handleValidHashParameter,
};

// FIXME(jparent): Eventually remove all usage of global history object.
var g_history = new history.History(aggregateResultsConfig);
g_history.parseCrossDashboardParameters();

g_totalFailureCounts = {};

function totalFailureCountFor(builder)
{
    if (!g_totalFailureCounts[builder])
        g_totalFailureCounts[builder] = results.testCounts(g_resultsByBuilder[builder][results.NUM_FAILURES_BY_TYPE]);
    return g_totalFailureCounts[builder];
}

function htmlForBuilder(builder)
{
    var html = '<div class=container><h2>' + builder + '</h2>';

    if (g_history.dashboardSpecificState.rawValues) {
        html += htmlForTestType(builder);
    } else {
        html += '<a href="timeline_explorer.html' + (location.hash ? location.hash + '&' : '#') + 'builder=' + builder + '">' +
            chartHTML(builder) + '</a>';
    }

    return html + '</div>';
}

function chartHTML(builder)
{
    var resultsForBuilder = g_resultsByBuilder[builder];
    var totalFailingTests = totalFailureCountFor(builder).totalFailingTests;

    // Some bots don't properly record revision numbers. Handle that gracefully.
    var label, values;
    if (currentBuilderGroup().isToTBlink && resultsForBuilder[results.BLINK_REVISIONS]) {
        label = 'Blink Revision';
        values = resultsForBuilder[results.BLINK_REVISIONS]
    } else if (resultsForBuilder[results.CHROME_REVISIONS]) {
        label = 'Chrome Revision';
        values = resultsForBuilder[results.CHROME_REVISIONS];
    } else {
        label = 'Build Number';
        values = resultsForBuilder[results.BUILD_NUMBERS];
    }

    var start = values[totalFailingTests.length - 1];
    var end = values[0];
    var html = chart("Total failing", {"": totalFailingTests}, label, start, end);

    var values = resultsForBuilder[results.NUM_FAILURES_BY_TYPE];
    // Don't care about number of passes for the charts.
    delete(values[results.PASS]);

    return html + chart("Detailed breakdown", values, label, start, end);
}

var LABEL_COLORS = ['FF0000', '00FF00', '0000FF', '000000', 'FF6EB4', 'FFA812', '9B30FF', '00FFCC'];

// FIXME: Find a better way to exclude outliers. This is just so we exclude
// runs where every test failed.
var MAX_VALUE = 2000;

function filteredValues(values, desiredNumberOfPoints)
{
    // Filter out values to make the graph a bit more readable and to keep URLs
    // from exceeding the browsers max length restriction.
    var filterAmount = Math.floor(values.length / desiredNumberOfPoints);
    return values.filter(function(element, index, array) {
        if (!g_history.dashboardSpecificState.showOutliers && element > MAX_VALUE)
            return false;
        if (filterAmount <= 1)
            return true;
        // Include the most recent and oldest values.
        return index % filterAmount == 0 || index == array.length - 1;
    });
}

function isAllZeros(values)
{
    return !values.some(function(element) { return element > 0; });
}

function chartUrl(title, values, revisionLabel, startRevision, endRevision, desiredNumberOfPoints)
{
    var maxValue = 0;
    for (var expectation in values)
        maxValue = Math.max(maxValue, Math.max.apply(null, filteredValues(values[expectation], desiredNumberOfPoints)));

    var chartData = '';
    var labels = '';
    var numLabels = 0;

    for (var expectation in values) {
        if (expectation == 'WONTFIX' || isAllZeros(values[expectation]))
            continue;
        chartData += (chartData ? ',' : 'e:') + extendedEncode(filteredValues(values[expectation], desiredNumberOfPoints).reverse(), maxValue);

        if (expectation) {
            numLabels++;
            labels += (labels ? '|' : '') + expectation;
        }
    }

    var url = "http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=" +
            chartData + "&chg=15,15,1,3&chxt=x,x,y&chxl=1:||" + revisionLabel +
            "|&chxr=0," + startRevision + "," + endRevision + "|2,0," + maxValue + "&chtt=" + title;

    if (labels)
        url += "&chdl=" + labels + "&chco=" + LABEL_COLORS.slice(0, numLabels).join(',');
    return url;
}

function chart(title, values, revisionLabel, startRevision, endRevision)
{
    var desiredNumberOfPoints = 400;
    var url = chartUrl(title, values, revisionLabel, startRevision, endRevision, desiredNumberOfPoints);

    while (url.length >= 2048) {
        // Decrease the desired number of points gradually until we get a URL that
        // doesn't exceed chartserver's max URL length.
        desiredNumberOfPoints = 3 / 4 * desiredNumberOfPoints;
        url = chartUrl(title, values, revisionLabel, startRevision, endRevision, desiredNumberOfPoints);
    }

    return '<img src="' + url + '">';
}

function htmlForRevisionRows(resultsForBuilder, numColumns)
{
    var html = '';
    if (resultsForBuilder[results.BLINK_REVISIONS])
        html += htmlForTableRow('Blink Revision', resultsForBuilder[results.BLINK_REVISIONS].slice(0, numColumns));
    if (resultsForBuilder[results.CHROME_REVISIONS])
        html += htmlForTableRow('Chrome Revision', resultsForBuilder[results.CHROME_REVISIONS].slice(0, numColumns));
    return html;
}

function htmlForTestType(builder)
{
    var counts = totalFailureCountFor(builder);
    var totalFailing = counts.totalFailingTests;
    var totalTests = counts.totalTests;

    var percent = [];
    for (var i = 0; i < totalTests.length; i++) {
        var percentage = 100 * (totalTests[i] - totalFailing[i]) / totalTests[i];
        // Round to the nearest tenth of a percent.
        percent.push(Math.round(percentage * 10) / 10 + '%');
    }

    var resultsForBuilder = g_resultsByBuilder[builder];
    html = '<table><tbody>' +
        htmlForRevisionRows(resultsForBuilder, totalTests.length) +
        htmlForTableRow('Percent passed', percent) +
        htmlForTableRow('Failures', totalFailing) +
        htmlForTableRow('Total Tests', totalTests);

    var values = resultsForBuilder[results.NUM_FAILURES_BY_TYPE];
    for (var expectation in values)
        html += htmlForTableRow(expectation, values[expectation]);

    return html + '</tbody></table>';
}

function htmlForTableRow(columnName, values)
{
    return '<tr><td>' + columnName + '</td><td>' + values.join('</td><td>') + '</td></tr>';
}

// Taken from http://code.google.com/apis/chart/docs/data_formats.html.
function extendedEncode(arrVals, maxVal)
{
    var map = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-.';
    var mapLength = map.length;
    var mapLengthSquared = mapLength * mapLength;

    var chartData = '';

    for (var i = 0, len = arrVals.length; i < len; i++) {
        // In case the array vals were translated to strings.
        var numericVal = new Number(arrVals[i]);
        // Scale the value to maxVal.
        var scaledVal = Math.floor(mapLengthSquared * numericVal / maxVal);

        if(scaledVal > mapLengthSquared - 1)
            chartData += "..";
        else if (scaledVal < 0)
            chartData += '__';
        else {
            // Calculate first and second digits and add them to the output.
            var quotient = Math.floor(scaledVal / mapLength);
            var remainder = scaledVal - mapLength * quotient;
            chartData += map.charAt(quotient) + map.charAt(remainder);
        }
    }

    return chartData;
}

window.addEventListener('load', function() {
    var resourceLoader = new loader.Loader();
    resourceLoader.load();
}, false);
