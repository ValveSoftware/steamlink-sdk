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
