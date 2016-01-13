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

var ui = ui || {};

(function() {

ui.popup = {};

ui.popup.hide = function()
{
    var popup = $('popup');
    if (popup) {
        popup.parentNode.removeChild(popup);
        document.removeEventListener('mousedown', ui.popup._handleMouseDown, false);
    }
}

ui.popup.show = function(target, html)
{
    var popup = $('popup');
    if (!popup) {
        popup = document.createElement('div');
        popup.id = 'popup';
        document.body.appendChild(popup);
        document.addEventListener('mousedown', ui.popup._handleMouseDown, false);
    }

    // Set html first so that we can get accurate size metrics on the popup.
    popup.innerHTML = html;

    var targetRect = target.getBoundingClientRect();

    var x = Math.min(targetRect.left - 10, document.documentElement.clientWidth - popup.offsetWidth);
    x = Math.max(0, x);
    popup.style.left = x + document.body.scrollLeft + 'px';

    var y = targetRect.top + targetRect.height;
    if (y + popup.offsetHeight > document.documentElement.clientHeight)
        y = targetRect.top - popup.offsetHeight;
    y = Math.max(0, y);
    popup.style.top = y + document.body.scrollTop + 'px';
}

ui.popup._handleMouseDown = function(e) {
    // Clear the open popup, unless the click was inside the popup.
    var popup = $('popup');
    if (popup && e.target != popup && !(popup.compareDocumentPosition(e.target) & 16))
        ui.popup.hide();
}

ui.html = {};

ui.html.checkbox = function(queryParameter, label, isChecked, opt_extraJavaScript)
{
    var js = opt_extraJavaScript || '';
    return '<label style="padding-left: 2em">' +
        '<input type="checkbox" onchange="g_history.toggleQueryParameter(\'' + queryParameter + '\');' + js + '" ' +
            (isChecked ? 'checked' : '') + '>' + label +
        '</label>';
}

ui.html.range = function(queryParameter, label, min, max, initialValue)
{
    return '<label>' +
        label +
        '<input type=range onchange="g_history.setQueryParameter(\'' + queryParameter + '\', this.value)" min=' + min + ' max=' + max + ' value=' + initialValue + '>' +
    '</label>';
}

ui.html.select = function(label, queryParameter, options)
{
    var html = '<label style="padding-left: 2em">' + label + ': ' +
        '<select onchange="g_history.setQueryParameter(\'' + queryParameter + '\', this[this.selectedIndex].value)">';

    for (var i = 0; i < options.length; i++) {
        var value = options[i];
        html += '<option value="' + value + '" ' +
            (g_history.queryParameterValue(queryParameter) == value ? 'selected' : '') +
            '>' + value + '</option>'
    }
    html += '</select></label> ';
    return html;
}

ui.html.navbar = function(opt_extraHtml)
{
    var html = '<div style="border-bottom:1px dashed">';
    html = ui.html._dashboardLink('Overview', 'overview.html') +
        ui.html._dashboardLink('Results', 'flakiness_dashboard.html') +
        ui.html._dashboardLink('Times', 'treemap.html') +
        ui.html._dashboardLink('Stats', 'aggregate_results.html') +
        ui.html._dashboardLink('Stats Timeline', 'timeline_explorer.html');

    if (opt_extraHtml)
        html += opt_extraHtml;

    if (!history.isTreeMap())
        html += ui.html.checkbox('showAllRuns', 'Use all recorded runs', g_history.crossDashboardState.showAllRuns);

    return html + '</div>';
}

// Returns the HTML for the select element to switch to different testTypes.
ui.html.testTypeSwitcher = function(opt_noBuilderMenu, opt_extraHtml, opt_includeNoneBuilder)
{
    var html = ui.html.select('Test type', 'testType', builders.testTypes);
    if (!opt_noBuilderMenu) {
        var buildersForMenu = Object.keys(currentBuilders());
        if (opt_includeNoneBuilder)
            buildersForMenu.unshift('--------------');
        html += ui.html.select('Builder', 'builder', buildersForMenu);
    }

    html += ui.html.select('Group', 'group', builders.groupNamesForTestType(g_history.crossDashboardState.testType));

    if (opt_extraHtml)
        html += opt_extraHtml;
    return ui.html.navbar(html);
}

ui.html._loadDashboard = function(fileName)
{
    var pathName = window.location.pathname;
    pathName = pathName.substring(0, pathName.lastIndexOf('/') + 1);
    window.location = pathName + fileName + window.location.hash;
}

ui.html._topLink = function(html, onClick, isSelected)
{
    var cssText = isSelected ? 'font-weight: bold;' : 'color:blue;text-decoration:underline;cursor:pointer;';
    cssText += 'margin: 0 5px;';
    return '<span style="' + cssText + '" onclick="' + onClick + '">' + html + '</span>';
}

ui.html._dashboardLink = function(html, fileName)
{
    var pathName = window.location.pathname;
    var currentFileName = pathName.substring(pathName.lastIndexOf('/') + 1);
    var isSelected = currentFileName == fileName;
    var onClick = 'ui.html._loadDashboard(\'' + fileName + '\')';
    return ui.html._topLink(html, onClick, isSelected);
}

ui.html._revisionLink = function(resultsKey, testResults, index)
{
    var currentRevision = parseInt(testResults[resultsKey][index], 10);
    var previousRevision = parseInt(testResults[resultsKey][index + 1], 10);

    var isChrome = resultsKey == results.CHROME_REVISIONS;
    var singleUrl = 'http://src.chromium.org/viewvc/' + (isChrome ? 'chrome' : 'blink') + '?view=rev&revision=' + currentRevision;

    if (currentRevision == previousRevision)
        return 'At <a href="' + singleUrl + '">r' + currentRevision    + '</a>';

    if (currentRevision - previousRevision == 1)
        return '<a href="' + singleUrl + '">r' + currentRevision    + '</a>';

    var rangeUrl = 'http://build.chromium.org/f/chromium/perf/dashboard/ui/changelog' +
        (isChrome ? '' : '_blink') + '.html?url=/trunk' + (isChrome ? '/src' : '') +
        '&range=' + (previousRevision + 1) + ':' + currentRevision + '&mode=html';
    return '<a href="' + rangeUrl + '">r' + (previousRevision + 1) + ' to r' + currentRevision + '</a>';
}

ui.html.chromiumRevisionLink = function(testResults, index)
{
    return ui.html._revisionLink(results.CHROME_REVISIONS, testResults, index);
}

ui.html.blinkRevisionLink = function(testResults, index)
{
    return ui.html._revisionLink(results.BLINK_REVISIONS, testResults, index);
}


ui.Errors = function() {
    this._messages = '';
    // Element to display the errors within.
    this._containerElement = null;
}

ui.Errors.prototype = {
    show: function()
    {
        if (!this._containerElement) {
            this._containerElement = document.createElement('H2');
            this._containerElement.style.color = 'red';
            this._containerElement.id = 'errors';
            document.documentElement.insertBefore(this._containerElement, document.body);
        }

        this._containerElement.innerHTML = this._messages;
    },
    // Record a new error message.
    addError: function(message)
    {
        this._messages += message + '<br>';
    },
    hasErrors: function()
    {
        return !!this._messages;
    }
}

})();
