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
    width: 200
    height: 400

    property var newViewRequest: null
    property var dialog: null

    SignalSpy {
        id: newViewRequestedSpy
        target: webEngineView
        signalName: "newViewRequested"
    }

    onNewViewRequested: {
        newViewRequest = {
            "destination": request.destination,
            "userInitiated": request.userInitiated
        };

        dialog = Qt.createQmlObject(
            "import QtQuick.Window 2.0\n" +
            "Window {\n" +
            "    width: 100; height: 100\n" +
            "    visible: true; flags: Qt.Dialog\n" +
            "    property alias webEngineView: webView\n" +
            "    TestWebEngineView { id: webView; anchors.fill: parent }\n" +
            "}", webEngineView);

        request.openIn(dialog.webEngineView);
    }

    TestCase {
        id: test
        name: "NewViewRequest"
        when: windowShown

        function init() {
            webEngineView.url = Qt.resolvedUrl("about:blank");
            verify(webEngineView.waitForLoadSucceeded());

            newViewRequestedSpy.clear();
            newViewRequest = null;
        }

        function cleanup() {
            if (dialog)
                dialog.destroy();
        }

        function test_jsWindowOpen() {
            // Open an empty page in a new tab
            webEngineView.loadHtml(
                "<html><head><script>" +
                "   function popup() { window.open(''); }" +
                "</script></head>" +
                "<body onload='popup()'></body></html>");
            verify(webEngineView.waitForLoadSucceeded());
            tryCompare(newViewRequestedSpy, "count", 1);

            compare(newViewRequest.destination, WebEngineView.NewViewInTab);
            verify(!newViewRequest.userInitiated);

            verify(dialog.webEngineView.waitForLoadSucceeded);
            compare(dialog.webEngineView.url, "");
            newViewRequestedSpy.clear();
            dialog.destroy();

            // Open an empty page in a new dialog
            webEngineView.loadHtml(
                "<html><head><script>" +
                "   function popup() { window.open('', '_blank', 'width=200,height=100'); }" +
                "</script></head>" +
                "<body onload='popup()'></body></html>");
            verify(webEngineView.waitForLoadSucceeded());
            tryCompare(newViewRequestedSpy, "count", 1);

            compare(newViewRequest.destination, WebEngineView.NewViewInDialog);
            verify(!newViewRequest.userInitiated);
            verify(dialog.webEngineView.waitForLoadSucceeded);
            newViewRequestedSpy.clear();
            dialog.destroy();

            // Open an empty page in a new dialog by user
            webEngineView.loadHtml(
                "<html><head><script>" +
                "   function popup() { window.open('', '_blank', 'width=200,height=100'); }" +
                "</script></head>" +
                "<body onload=\"document.getElementById('popupButton').focus();\">" +
                "   <button id='popupButton' onclick='popup()'>Pop Up!</button>" +
                "</body></html>");
            verify(webEngineView.waitForLoadSucceeded());
            verifyElementHasFocus("popupButton");
            keyPress(Qt.Key_Enter);
            tryCompare(newViewRequestedSpy, "count", 1);

            compare(newViewRequest.destination, WebEngineView.NewViewInDialog);
            verify(newViewRequest.userInitiated);
            verify(dialog.webEngineView.waitForLoadSucceeded);
            newViewRequestedSpy.clear();
            dialog.destroy();
        }
    }
}

