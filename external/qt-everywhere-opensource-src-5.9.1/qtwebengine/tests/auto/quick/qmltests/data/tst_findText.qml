/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtWebEngine 1.2

TestWebEngineView {
    id: webEngineView
    width: 400
    height: 300

    property int matchCount: 0
    property bool findFailed: false

    function clear() {
        findFailed = false
        matchCount = -1
    }

    function findTextCallback(matchCount) {
        webEngineView.matchCount = matchCount
        findFailed = matchCount == 0
    }


    TestCase {
        name: "WebViewFindText"

        function getBodyInnerHTML() {
            var bodyInnerHTML;
            runJavaScript("document.body.innerHTML", function(result) {
                bodyInnerHTML = result;
            });
            tryVerify(function() { return bodyInnerHTML != undefined; });
            return bodyInnerHTML;
        }

        function getListItemText(index) {
            var listItemText;
            runJavaScript("document.getElementById('list').getElementsByTagName('li')[" + index + "].innerText;", function(result) {
                listItemText = result;
            });
            tryVerify(function() { return listItemText != undefined; });
            return listItemText;
        }

        function appendListItem(text) {
            var script =
                    "(function () {" +
                    "   var list = document.getElementById('list');" +
                    "   var item = document.createElement('li');" +
                    "   item.appendChild(document.createTextNode('" + text + "'));" +
                    "   list.appendChild(item);" +
                    "   return list.getElementsByTagName('li').length - 1;" +
                    "})();";
            var itemIndex;

            runJavaScript(script, function(result) { itemIndex = result; });
            tryVerify(function() { return itemIndex != undefined; });
            // Make sure the DOM is up-to-date.
            tryVerify(function() { return getListItemText(itemIndex).length == text.length; });
        }

        function test_findText() {
            var findFlags = WebEngineView.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("Hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextCaseInsensitive() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextManyMatches() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test4.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("bla", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 100)
            verify(!findFailed)
        }


        function test_findTextFailCaseSensitive() {
            var findFlags = WebEngineView.FindCaseSensitively
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("heLLo", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }

        function test_findTextNotFound() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("string-that-is-not-threre", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)
        }

        function test_findTextAfterNotFound() {
            var findFlags = 0
            webEngineView.url = Qt.resolvedUrl("about:blank")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)

            webEngineView.url = Qt.resolvedUrl("test1.html")
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextInModifiedDOMAfterNotFound() {
            var findFlags = 0
            webEngineView.loadHtml(
                        "<html><body>" +
                        "bla" +
                        "</body></html>");
            verify(webEngineView.waitForLoadSucceeded())

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 0)
            verify(findFailed)

            runJavaScript("document.body.innerHTML = 'blahellobla'");
            tryVerify(function() { return getBodyInnerHTML() == "blahellobla"; });

            webEngineView.clear()
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback)
            tryCompare(webEngineView, "matchCount", 1)
            verify(!findFailed)
        }

        function test_findTextInterruptedByLoad() {
            var findFlags = 0;

            var listItemText = '';
            for (var i = 0; i < 100000; ++i)
                listItemText += "bla ";
            listItemText = listItemText.trim();

            webEngineView.loadHtml(
                        "<html><body>" +
                        "<ol id='list' />" +
                        "</body></html>");
            verify(webEngineView.waitForLoadSucceeded());

            // Generating a huge list is a workaround to avoid timeout while loading the test page.
            for (var i = 0; i < 10; ++i)
                appendListItem(listItemText);
            appendListItem("hello");

            webEngineView.clear();
            webEngineView.findText("hello", findFlags, webEngineView.findTextCallback);

            // This should not crash.
            webEngineView.url = "https://www.qt.io";
            if (!webEngineView.waitForLoadSucceeded(12000))
                skip("Couldn't load page from network, skipping test.");

            // Can't be sure whether the findText succeeded before the new load.
            // Thus don't check the find result just whether the callback was called.
            tryVerify(function() { return webEngineView.matchCount != -1; });
        }
    }
}
