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
import QtTest 1.1

TestCase {
    name: "SelfTests_compare_QuickObjects"
    id: testParent

    Rectangle {
        id: item1
        color: "#000000"
    }
    Rectangle {
        id: item2
        color: "#000000"
    }
    Rectangle {
        id: item3
        color: "#ffffff"
    }

    Component {
        id: item4

        Rectangle {
            color: "#ffffff"
        }
    }

    function test_quickobjects() {
        compare(qtest_compareInternal(item1, item1), true, "Identical QtQuick instances");
        compare(qtest_compareInternal(item1, item3), false, "QtQuick instances with different properties");

        expectFail("", "Unsure if we want this.");
        compare(qtest_compareInternal(item1, item2), true, "QtQuick instances with same properties");

        expectFail("", "Unsure if we want this.");
        compare(qtest_compareInternal(item4.createObject(testParent),
                                      item4.createObject(testParent)), true, "QtQuick dynamic instances");
    }
}
