/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCanvas3D module of the Qt Toolkit.
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

import QtQuick 2.2
import QtCanvas3D 1.1
import QtTest 1.0

import "tst_render_target.js" as Content

Item {
    id: top
    height: 300
    width: 300
    property var canvas3d: null
    property var activeContent: Content
    property bool renderOk: false

    Rectangle {
        id: backgroundRect1
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: parent.width / 2
        color: "#FF00FF"
    }
    Rectangle {
        id: backgroundRect2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width / 2
        color: "#FF00FF"
        visible: false
    }

    function createBackgroundCanvas() {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.1
        Canvas3D {
            renderTarget: Canvas3D.RenderTargetBackground
            onInitializeGL: activeContent.initializeGL(canvas3d)
            onPaintGL: {
                activeContent.paintGL(true)
                renderOk = true
            }
        }", top)
        canvas3d.anchors.fill = top
    }


    function createForegroundCanvas() {
        canvas3d = Qt.createQmlObject("
        import QtQuick 2.2
        import QtCanvas3D 1.1
        Canvas3D {
            renderTarget: Canvas3D.RenderTargetForeground
            onInitializeGL: activeContent.initializeGL(canvas3d)
            onPaintGL: {
                activeContent.paintGL(false)
                renderOk = true
            }
        }", top)
        canvas3d.anchors.fill = top
    }

    TestCase {
        name: "Canvas3D_render_target"
        when: windowShown

        // Check that background canvas gets rendered on background
        function test_render_1_target() {
            createBackgroundCanvas()
            renderOk = false
            waitForRendering(canvas3d)
            tryCompare(top, "renderOk", true)
            var image = grabImage(top)
            verify(image.pixel(10, 150) === Qt.rgba(1, 0, 0, 1)) // clear color
            verify(image.pixel(100, 150) === Qt.rgba(0, 1, 1, 1)) // canvas bg quad
            verify(image.pixel(200, 150) === Qt.rgba(1, 0, 1, 1)) // backgroundRect1
            verify(image.pixel(290, 150) === Qt.rgba(1, 0, 1, 1)) // backgroundRect1
            canvas3d.destroy();
        }

        // Check that foreground canvas gets rendered on foreground
        function test_render_2_target() {
            // Show the other background rect so we have something to show through everywhere
            backgroundRect2.visible = true
            createForegroundCanvas()
            renderOk = false
            waitForRendering(canvas3d)
            tryCompare(top, "renderOk", true)
            var image = grabImage(top)
            verify(image.pixel(10, 150) === Qt.rgba(1, 0, 1, 1)) // backgroundRect2
            verify(image.pixel(100, 150) === Qt.rgba(1, 1, 0, 1)) // canvas fg quad
            verify(image.pixel(200, 150) === Qt.rgba(1, 1, 0, 1)) // canvas fg quad
            verify(image.pixel(290, 150) === Qt.rgba(1, 0, 1, 1)) // backgroundRect1
            canvas3d.destroy();
        }
    }
}
