/*********************************************************************
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
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

import QtWebChannel 1.0

Item {
    id: test
    signal barCalled(var arg)
    signal clientInitializedCalled(var arg)

    QtObject {
        id: testObject
        WebChannel.id: "testObject"

        property var foo: 42

        function clientInitialized(arg)
        {
            clientInitializedCalled(arg);
        }

        function bar(arg) {
            barCalled(arg);
        }

        signal runTest(var foo)
    }

    TestWebEngineView {
        id: webView
        webChannel.registeredObjects: [testObject]
    }

    SignalSpy {
        id: initializedSpy
        target: test
        signalName: "clientInitializedCalled"
    }

    SignalSpy {
        id: barSpy
        target: test
        signalName: "barCalled"
    }

    TestCase {
        name: "WebViewWebChannel"
        property url testUrl: Qt.resolvedUrl("./webchannel-test.html")

        function init() {
            initializedSpy.clear();
            barSpy.clear();
        }

        function test_basic() {
            webView.url = testUrl;
            verify(webView.waitForLoadSucceeded());

            initializedSpy.wait();
            compare(initializedSpy.signalArguments.length, 1);
            compare(initializedSpy.signalArguments[0][0], 42);

            var newValue = "roundtrip";
            testObject.runTest(newValue);
            barSpy.wait();
            compare(barSpy.signalArguments.length, 1);
            compare(barSpy.signalArguments[0][0], newValue);

            compare(testObject.foo, newValue);
        }
    }
}
