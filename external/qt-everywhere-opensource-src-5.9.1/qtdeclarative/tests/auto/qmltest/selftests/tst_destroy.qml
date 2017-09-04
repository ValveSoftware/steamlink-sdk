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

import QtQuick 2.1
import QtTest 1.1

TestCase {
    id: testCase
    width: 100
    height: 100
    name: "SelfTests_Destroy"

    function test_a_QTBUG_30523() {
        compare(testCase.children.length, 0)
        var tmp = Qt.createQmlObject('import QtQuick 2.1; Rectangle {width: 20; height: 20;}', testCase, '')
        compare(testCase.children.length, 1)
        tmp.destroy()
    }

    function test_b_QTBUG_30523() {
        // The object created in test above should be deleted
        compare(testCase.children.length, 0)
    }

    function test_c_QTBUG_42185_data() {
   // Adding dummy data objects for the purpose of calling multiple times the test function
   // and checking that the created object (tmp) is destroyed as expected between each data run.
        return [{tag: "test 0"}, {tag: "test 1"}, {tag: "test 2"},];
    }

    function test_c_QTBUG_42185() {
        compare(testCase.children.length, 0)
        var tmp = Qt.createQmlObject('import QtQuick 2.1; Rectangle {width: 20; height: 20;}', testCase, '')
        compare(testCase.children.length, 1)
        tmp.destroy()
    }

    function test_d_QTBUG_42185() {
        // The object created in test above should be deleted
        compare(testCase.children.length, 0)
    }
}
