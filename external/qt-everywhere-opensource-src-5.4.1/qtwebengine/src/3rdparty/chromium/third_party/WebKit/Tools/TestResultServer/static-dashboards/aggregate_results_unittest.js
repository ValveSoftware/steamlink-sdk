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

module('aggregate_results');

function setupAggregateResultsData(includeRevisonNumbers)
{
    var historyInstance = new history.History(flakinessConfig);
    // FIXME(jparent): Remove this once global isn't used.
    g_history = historyInstance;
    for (var key in history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES)
        historyInstance.crossDashboardState[key] = history.DEFAULT_CROSS_DASHBOARD_STATE_VALUES[key];

    var builderName = 'Blink Linux';
    LOAD_BUILDBOT_DATA({
        "no_upload_test_types": [],
        "masters": [
            {
                "groups": [ "@ToT Blink" ],
                "name": "ChromiumWebkit",
                "url_name": "chromium.webkit",
                "tests": {
                    "layout-tests": {
                        "builders": [builderName]
                    }
                },
            }
        ]
    });
    builders.loadBuildersList('@ToT Blink', 'layout-tests');

    g_resultsByBuilder[builderName] = {
        "num_failures_by_type": {
            "CRASH": [ 13, 10 ],
            "MISSING": [ 6, 8 ],
            "IMAGE+TEXT": [ 17, 17 ],
            "IMAGE": [ 81, 68 ],
            "SKIP": [ 1167, 748 ],
            "TEXT": [ 89, 60 ],
            "TIMEOUT": [ 72, 48 ],
            "PASS": [ 28104, 28586 ],
            "AUDIO": [ 0, 0 ],
            "WONTFIX": [ 2, 2 ],
        },
        "buildNumbers": [5, 3]
    }

    if (includeRevisonNumbers) {
        g_resultsByBuilder[builderName][results.BLINK_REVISIONS] = [1234, 1233];
        g_resultsByBuilder[builderName][results.CHROME_REVISIONS] = [4567, 4566];
    }

    g_totalFailureCounts = {};
}

test('htmlForBuilderIncludeRevisionNumbers', 1, function() {
    var includeRevisonNumbers = true;
    setupAggregateResultsData(includeRevisonNumbers);
    g_history.dashboardSpecificState.rawValues = false;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2>' +
        '<a href="timeline_explorer.html#useTestData=true&builder=Blink Linux">' +
            '<img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:qe..&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Blink Revision|&chxr=0,1233,1234|2,0,1445&chtt=Total failing">' +
            '<img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:AjAt,AcAV,A7A7,DuEc,pB..,DSE4,CoD8&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Blink Revision|&chxr=0,1233,1234|2,0,1167&chtt=Detailed breakdown&chdl=CRASH|MISSING|IMAGE+TEXT|IMAGE|SKIP|TEXT|TIMEOUT&chco=FF0000,00FF00,0000FF,000000,FF6EB4,FFA812,9B30FF">' +
        '</a>' +
    '</div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});

test('htmlForBuilder', 1, function() {
    var includeRevisonNumbers = false;
    setupAggregateResultsData(includeRevisonNumbers);
    g_history.dashboardSpecificState.rawValues = false;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2>' +
        '<a href="timeline_explorer.html#useTestData=true&builder=Blink Linux">' +
            '<img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:qe..&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Build Number|&chxr=0,3,5|2,0,1445&chtt=Total failing">' +
            '<img src="http://chart.apis.google.com/chart?cht=lc&chs=600x400&chd=e:AjAt,AcAV,A7A7,DuEc,pB..,DSE4,CoD8&chg=15,15,1,3&chxt=x,x,y&chxl=1:||Build Number|&chxr=0,3,5|2,0,1167&chtt=Detailed breakdown&chdl=CRASH|MISSING|IMAGE+TEXT|IMAGE|SKIP|TEXT|TIMEOUT&chco=FF0000,00FF00,0000FF,000000,FF6EB4,FFA812,9B30FF">' +
        '</a>' +
    '</div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});

test('htmlForBuilderRawResultsIncludeRevisionNumbers', 1, function() {
    var includeRevisonNumbers = true;
    setupAggregateResultsData(includeRevisonNumbers);
    g_history.dashboardSpecificState.rawValues = true;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2>' +
        '<table>' +
            '<tbody>' +
                '<tr><td>Blink Revision</td><td>1234</td><td>1233</td></tr>' +
                '<tr><td>Chrome Revision</td><td>4567</td><td>4566</td></tr>' +
                '<tr><td>Percent passed</td><td>95.1%</td><td>96.8%</td></tr>' +
                '<tr><td>Failures</td><td>1445</td><td>959</td></tr>' +
                '<tr><td>Total Tests</td><td>29549</td><td>29545</td></tr>' +
                '<tr><td>CRASH</td><td>13</td><td>10</td></tr>' +
                '<tr><td>MISSING</td><td>6</td><td>8</td></tr>' +
                '<tr><td>IMAGE+TEXT</td><td>17</td><td>17</td></tr>' +
                '<tr><td>IMAGE</td><td>81</td><td>68</td></tr>' +
                '<tr><td>SKIP</td><td>1167</td><td>748</td></tr>' +
                '<tr><td>TEXT</td><td>89</td><td>60</td></tr>' +
                '<tr><td>TIMEOUT</td><td>72</td><td>48</td></tr>' +
                '<tr><td>PASS</td><td>28104</td><td>28586</td></tr>' +
                '<tr><td>AUDIO</td><td>0</td><td>0</td></tr>' +
                '<tr><td>WONTFIX</td><td>2</td><td>2</td></tr>' +
            '</tbody>' +
        '</table>' +
    '</div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});

test('htmlForBuilderRawResults', 1, function() {
    var includeRevisonNumbers = false;
    setupAggregateResultsData(includeRevisonNumbers);
    g_history.dashboardSpecificState.rawValues = true;

    var expectedHtml = '<div class=container>' +
        '<h2>Blink Linux</h2>' +
        '<table>' +
            '<tbody>' +
                '<tr><td>Percent passed</td><td>95.1%</td><td>96.8%</td></tr>' +
                '<tr><td>Failures</td><td>1445</td><td>959</td></tr>' +
                '<tr><td>Total Tests</td><td>29549</td><td>29545</td></tr>' +
                '<tr><td>CRASH</td><td>13</td><td>10</td></tr>' +
                '<tr><td>MISSING</td><td>6</td><td>8</td></tr>' +
                '<tr><td>IMAGE+TEXT</td><td>17</td><td>17</td></tr>' +
                '<tr><td>IMAGE</td><td>81</td><td>68</td></tr>' +
                '<tr><td>SKIP</td><td>1167</td><td>748</td></tr>' +
                '<tr><td>TEXT</td><td>89</td><td>60</td></tr>' +
                '<tr><td>TIMEOUT</td><td>72</td><td>48</td></tr>' +
                '<tr><td>PASS</td><td>28104</td><td>28586</td></tr>' +
                '<tr><td>AUDIO</td><td>0</td><td>0</td></tr>' +
                '<tr><td>WONTFIX</td><td>2</td><td>2</td></tr>' +
            '</tbody>' +
        '</table>' +
    '</div>';
    equal(expectedHtml, htmlForBuilder('Blink Linux'));
});
