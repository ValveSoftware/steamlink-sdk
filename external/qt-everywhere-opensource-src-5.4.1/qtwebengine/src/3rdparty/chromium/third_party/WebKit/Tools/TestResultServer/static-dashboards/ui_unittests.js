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

module('ui');

test('ui.html.range', 1, function() {
    equal(ui.html.range('mockParameter', 'mockLabel', 1, 4, 2),
        '<label>mockLabel' +
            '<input type=range onchange="g_history.setQueryParameter(\'mockParameter\', this.value)" min=1 max=4 value=2>' +
        '</label>');
})

test('ui.html.navbar', 3, function() {
    var container = document.createElement('div');
    container.innerHTML = ui.html.navbar();
    equal(container.querySelectorAll('span').length, 5);
    equal(container.querySelectorAll('input').length, 1);

    container.innerHTML = ui.html.navbar('<div id="test-div"></div>');
    ok(container.querySelector('#test-div'));
})

test('chromiumRevisionLinkOneRevision', 1, function() {
    var testResults = {};
    testResults[results.CHROME_REVISIONS] = [3, 2, 1];
    var html = ui.html.chromiumRevisionLink(testResults, 1);
    equal(html, '<a href="http://src.chromium.org/viewvc/chrome?view=rev&revision=2">r2</a>');
});

test('chromiumRevisionLinkAtRevision', 1, function() {
    var testResults = {};
    testResults[results.CHROME_REVISIONS] = [3, 2, 2];
    var html = ui.html.chromiumRevisionLink(testResults, 1);
    equal(html, 'At <a href="http://src.chromium.org/viewvc/chrome?view=rev&revision=2">r2</a>');
});

test('chromiumRevisionLinkRevisionRange', 1, function() {
    var testResults = {};
    testResults[results.CHROME_REVISIONS] = [5, 2];
    var html = ui.html.chromiumRevisionLink(testResults, 0);
    equal(html, '<a href="http://build.chromium.org/f/chromium/perf/dashboard/ui/changelog.html?url=/trunk/src&range=3:5&mode=html">r3 to r5</a>');
});

test('blinkRevisionLinkOneRevision', 1, function() {
    var testResults = {};
    testResults[results.BLINK_REVISIONS] = [3, 2, 1];
    var html = ui.html.blinkRevisionLink(testResults, 1);
    equal(html, '<a href="http://src.chromium.org/viewvc/blink?view=rev&revision=2">r2</a>');
});

test('blinkRevisionLinkAtRevision', 1, function() {
    var testResults = {};
    testResults[results.BLINK_REVISIONS] = [3, 2, 2];
    var html = ui.html.blinkRevisionLink(testResults, 1);
    equal(html, 'At <a href="http://src.chromium.org/viewvc/blink?view=rev&revision=2">r2</a>');
});

test('blinkRevisionLinkRevisionRange', 1, function() {
    var testResults = {};
    testResults[results.BLINK_REVISIONS] = [5, 2];
    var html = ui.html.blinkRevisionLink(testResults, 0);
    equal(html, '<a href="http://build.chromium.org/f/chromium/perf/dashboard/ui/changelog_blink.html?url=/trunk&range=3:5&mode=html">r3 to r5</a>');
});
