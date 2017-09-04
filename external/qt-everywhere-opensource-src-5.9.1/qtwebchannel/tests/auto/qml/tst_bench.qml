/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

import QtWebChannel 1.0
import QtWebChannel.Tests 1.0

TestCase {
    name: "Bench"
    id: test

    Client {
        id: client
    }

    TestWebChannel {
        id: webChannel
        transports: [client.serverTransport]
    }

    Component {
        id: component
        QtObject {
            property var p0 : 0
            property var p1 : 0
            property var p2 : 0
            property var p3 : 0
            property var p4 : 0
            property var p5 : 0
            property var p6 : 0
            property var p7 : 0
            property var p8 : 0
            property var p9 : 0
            function m0(arg1, arg2) {}
            function m1(arg1, arg2) {}
            function m2(arg1, arg2) {}
            function m3(arg1, arg2) {}
            function m4(arg1, arg2) {}
            function m5(arg1, arg2) {}
            function m6(arg1, arg2) {}
            function m7(arg1, arg2) {}
            function m8(arg1, arg2) {}
            function m9(arg1, arg2) {}
            signal s0(var arg1, var arg2)
            signal s1(var arg1, var arg2)
            signal s2(var arg1, var arg2)
            signal s3(var arg1, var arg2)
            signal s4(var arg1, var arg2)
            signal s5(var arg1, var arg2)
            signal s6(var arg1, var arg2)
            signal s7(var arg1, var arg2)
            signal s8(var arg1, var arg2)
            signal s9(var arg1, var arg2)
        }
    }

    property var objects: ({})

    function initTestCase()
    {
        for (var i = 0; i < 100; ++i) {
            var id = "obj" + i;
            var properties = {objectName: id};
            objects[id] = component.createObject(test, properties);
        }

        webChannel.registerObjects(objects);
    }

    function cleanup()
    {
        client.cleanup();
    }

    function benchmark_init()
    {
        var channel = client.createChannel(function() {});
        client.awaitInit();
        client.awaitIdle();
    }
}
