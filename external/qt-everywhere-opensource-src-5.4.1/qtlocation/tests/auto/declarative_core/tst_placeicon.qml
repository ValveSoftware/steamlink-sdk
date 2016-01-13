/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "Icon"

    Icon { id: emptyIcon }

    function test_empty() {
        compare(emptyIcon.plugin, null);
        compare(emptyIcon.parameters.keys().length, 0)
    }


    Icon {
        id: qmlIconSingleUrl
    }

    function test_qmlSingleUrlIcon() {
        qmlIconSingleUrl.parameters.singleUrl = "http://example.com/icon.png"

        var u = qmlIconSingleUrl.url(Qt.size(64, 64));
        compare(u, "http://example.com/icon.png");

        u = qmlIconSingleUrl.url(Qt.size(20, 20));
        compare(u, "http://example.com/icon.png");

        qmlIconSingleUrl.parameters.singleUrl = "/home/user/icon.png"
        u = qmlIconSingleUrl.url(Qt.size(20, 20));
        compare(u, "file:///home/user/icon.png");
    }

    Plugin {
        id: testPlugin
        name: "qmlgeo.test.plugin"
        allowExperimental: true
    }

    Icon {
        id: qmlIconParams
        plugin: testPlugin
    }

    function test_qmlIconParams() {
        compare(qmlIconParams.plugin, testPlugin);
        qmlIconParams.parameters.s = "http://example.com/icon_small.png"
        qmlIconParams.parameters.m = "http://example.com/icon_medium.png"
        qmlIconParams.parameters.l = "http://example.com/icon_large.png"

        compare(qmlIconParams.url(Qt.size(10, 10)), "http://example.com/icon_small.png");
        compare(qmlIconParams.url(Qt.size(20, 20)), "http://example.com/icon_small.png");
        compare(qmlIconParams.url(Qt.size(24, 24)), "http://example.com/icon_small.png");
        compare(qmlIconParams.url(Qt.size(25, 25)), "http://example.com/icon_medium.png");
        compare(qmlIconParams.url(Qt.size(30, 30)), "http://example.com/icon_medium.png");
        compare(qmlIconParams.url(Qt.size(39, 39)), "http://example.com/icon_medium.png");
        compare(qmlIconParams.url(Qt.size(40, 40)), "http://example.com/icon_large.png");
        compare(qmlIconParams.url(Qt.size(50, 50)), "http://example.com/icon_large.png");
        compare(qmlIconParams.url(Qt.size(60, 60)), "http://example.com/icon_large.png");
    }

    Icon {
        id: testIcon
    }

    function test_setAndGet_data() {
        return [
            { tag: "plugin", property: "plugin", signal: "pluginChanged", value: testPlugin },
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testIcon, data);
    }
}
