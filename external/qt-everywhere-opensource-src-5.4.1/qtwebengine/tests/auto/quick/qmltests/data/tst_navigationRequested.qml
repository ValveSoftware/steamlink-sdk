/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtWebEngine 1.0

TestWebEngineView {
    id: webEngineView
    width: 200
    height: 400

    property bool shouldIgnoreLinkClicks: false
    property bool shouldIgnoreSubFrameRequests: false

    QtObject {
        id: attributes
        property url mainUrl: ""
        property url iframeUrl: ""
        property bool linkClickedNavigationRequested: false
        property bool linkClickedNavigationIgnored: false

        function clear() {
            mainUrl = ""
            iframeUrl = ""
            linkClickedNavigationRequested = false
            linkClickedNavigationIgnored = false
        }
    }

    SignalSpy {
        id: navigationSpy
        target: webEngineView
        signalName: "navigationRequested"
    }

    onNavigationRequested: {
        if (request.isMainFrame) {
            attributes.mainUrl = request.url
        } else {
            attributes.iframeUrl = request.url
            if (shouldIgnoreSubFrameRequests) {
                request.action = WebEngineView.IgnoreRequest
            }
        }

        if (request.navigationType === WebEngineView.LinkClickedNavigation) {
            attributes.linkClickedNavigationRequested = true
            if (shouldIgnoreLinkClicks) {
                request.action = WebEngineView.IgnoreRequest
                attributes.linkClickedNavigationIgnored = true
            }
        }
    }

    TestCase {
        name: "WebEngineViewNavigationRequested"
        when: windowShown

        function init() {
            attributes.clear()
            navigationSpy.clear()
            shouldIgnoreLinkClicks = false
            shouldIgnoreSubFrameRequests = false
        }

        function test_navigationRequested() {
            // Test if we get notified about main frame and iframe loads
            compare(navigationSpy.count, 0)
            webEngineView.url = Qt.resolvedUrl("test2.html")
            navigationSpy.wait()
            compare(attributes.mainUrl, Qt.resolvedUrl("test2.html"))
            navigationSpy.wait()
            compare(attributes.iframeUrl, Qt.resolvedUrl("test1.html"))
            compare(navigationSpy.count, 2)
            verify(webEngineView.waitForLoadSucceeded())

            // Test if we get notified about clicked links
            mouseClick(webEngineView, 100, 100)
            tryCompare(navigationSpy, "count", 3)
            compare(attributes.mainUrl, Qt.resolvedUrl("test1.html"))
            verify(attributes.linkClickedNavigationRequested)
            verify(webEngineView.waitForLoadSucceeded())
        }

        function test_ignoreLinkClickedRequest() {
            // Test if we can ignore clicked link requests
            compare(navigationSpy.count, 0)
            webEngineView.url = Qt.resolvedUrl("test2.html")
            verify(webEngineView.waitForLoadSucceeded())

            shouldIgnoreLinkClicks = true
            mouseClick(webEngineView, 100, 100)
            tryCompare(navigationSpy, "count", 3)
            compare(attributes.mainUrl, Qt.resolvedUrl("test1.html"))
            verify(attributes.linkClickedNavigationRequested)
            verify(attributes.linkClickedNavigationIgnored)
            // We ignored the main frame request, so we should
            // get notified that the load has been stopped.
            verify(webEngineView.waitForLoadStopped())
            verify(!webEngineView.loading)
        }

        function test_ignoreSubFrameRequest() {
            // Test if we can ignore sub frame requests
            shouldIgnoreSubFrameRequests = true
            webEngineView.url = Qt.resolvedUrl("test2.html")
            tryCompare(navigationSpy, "count", 2)
            compare(attributes.mainUrl, Qt.resolvedUrl("test2.html"))
            compare(attributes.iframeUrl, Qt.resolvedUrl("test1.html"))
            // We ignored the sub frame request, so
            // the main frame load should still succeed.
            verify(webEngineView.waitForLoadSucceeded())
        }
    }
}
