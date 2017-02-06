/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
