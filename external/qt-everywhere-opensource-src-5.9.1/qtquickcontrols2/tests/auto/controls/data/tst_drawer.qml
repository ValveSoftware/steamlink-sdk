/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Controls 2.2

TestCase {
    id: testCase
    width: 400
    height: 400
    visible: true
    when: windowShown
    name: "Drawer"

    Component {
        id: drawer
        Drawer { }
    }

    function test_defaults() {
        var control = createTemporaryObject(drawer, testCase)
        compare(control.edge, Qt.LeftEdge)
        compare(control.position, 0.0)
        compare(control.dragMargin, Qt.styleHints.startDragDistance)
        compare(control.parent, ApplicationWindow.overlay)
    }

    Component {
        id: rectDrawer

        Drawer {
            Rectangle {
                width: 200
                height: 400
                color: "steelblue"
            }
        }
    }

    function test_swipeVelocity() {
        skip("QTBUG-52003");

        var control = createTemporaryObject(rectDrawer, testCase)
        verify(control.contentItem)
        compare(control.edge, Qt.LeftEdge)
        compare(control.position, 0.0)

        var dragDistance = Math.max(20, Qt.styleHints.startDragDistance + 5)
        var distance = dragDistance * 1.1
        if (distance >= control.width * 0.7)
            skip("This test requires a startDragDistance that is less than the opening threshold of the drawer")

        mousePress(control, 0, 0, Qt.LeftButton)
        mouseMove(control, distance, 0)
        verify(control.position > 0)
        tryCompare(control, "position", distance / control.contentItem.width)
        mouseRelease(control, distance, 0, Qt.LeftButton)
        tryCompare(control, "position", 1.0)
    }
}
