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
import QtWebEngine.testsupport 1.0

TestWebEngineView {
    id: webEngineView
    width: 200
    height: 400
    focus: true

    property string lastUrl

    testSupport: WebEngineTestSupport { }

    SignalSpy {
        id: loadVisuallyCommittedSpy
        target: webEngineView.testSupport
        signalName: "loadVisuallyCommitted"
    }

    SignalSpy {
        id: linkHoveredSpy
        target: webEngineView
        signalName: "linkHovered"
    }

    onLinkHovered: {
        webEngineView.lastUrl = hoveredUrl
    }

    TestCase {
        name: "DesktopWebEngineViewLinkHovered"

        // Delayed windowShown to workaround problems with Qt5 in debug mode.
        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function init() {
            webEngineView.lastUrl = "";
            loadVisuallyCommittedSpy.clear();
            linkHoveredSpy.clear();
        }

        function test_linkHovered() {
            compare(linkHoveredSpy.count, 0);
            mouseMove(webEngineView, 100, 300)
            webEngineView.url = Qt.resolvedUrl("test2.html")
            verify(webEngineView.waitForLoadSucceeded())

            // We get a linkHovered signal with empty hoveredUrl after page load
            linkHoveredSpy.wait();
            compare(linkHoveredSpy.count, 1);
            compare(webEngineView.lastUrl, "")

            // Wait for the page to be rendered before trying to test based on input events
            loadVisuallyCommittedSpy.wait();

            mouseMove(webEngineView, 100, 100)
            linkHoveredSpy.wait(12000);
            compare(linkHoveredSpy.count, 2);
            compare(webEngineView.lastUrl, Qt.resolvedUrl("test1.html"))
            mouseMove(webEngineView, 100, 300)
            linkHoveredSpy.wait(12000);
            compare(linkHoveredSpy.count, 3);
            compare(webEngineView.lastUrl, "")
        }

        function test_linkHoveredDoesntEmitRepeated() {
            compare(linkHoveredSpy.count, 0);
            webEngineView.url = Qt.resolvedUrl("test2.html")
            verify(webEngineView.waitForLoadSucceeded())

            // We get a linkHovered signal with empty hoveredUrl after page load
            linkHoveredSpy.wait();
            compare(linkHoveredSpy.count, 1);
            compare(webEngineView.lastUrl, "")

            // Wait for the page to be rendered before trying to test based on input events
            loadVisuallyCommittedSpy.wait();

            for (var i = 0; i < 100; i += 10)
                mouseMove(webEngineView, 100, 100 + i)

            linkHoveredSpy.wait(12000);
            compare(linkHoveredSpy.count, 2);
            compare(webEngineView.lastUrl, Qt.resolvedUrl("test1.html"))

            for (var i = 0; i < 100; i += 10)
                mouseMove(webEngineView, 100, 300 + i)

            linkHoveredSpy.wait(12000);
            compare(linkHoveredSpy.count, 3);
            compare(webEngineView.lastUrl, "")
        }
    }
}
