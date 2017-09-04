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

import "tst_render_ondemand.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property int xpos: 150
    property int ypos: 150
    property var pixels
    property int red: -1
    property int green: -1
    property int blue: -1
    property int alpha: -1
    property bool renderOk: false

    Canvas3D {
        id: canvas3d
        anchors.fill: parent
        renderOnDemand: true
        onInitializeGL: Content.initializeGL(this)

        onPaintGL: {
            pixels = Content.paintGL(xpos, ypos)
            red = pixels[0]
            green = pixels[1]
            blue = pixels[2]
            alpha = pixels[3]
            renderOk = true
        }
    }

    TestCase {
        name: "Canvas3D_render_ondemand"
        when: windowShown

        // Check that things get rendered on startup
        function test_render_1_ondemand() {
            // Check color in the center of the colored square
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x10)
            tryCompare(top, "green", 0x20)
            tryCompare(top, "blue", 0x30)
            tryCompare(top, "alpha", 0xff)
        }

        // Check that things get rendered when requested
        function test_render_2_ondemand() {
            // In case this test case is run by itself, clear the initial render
            tryCompare(top, "renderOk", true)

            renderOk = false
            Content.setColor(0x22, 0x99, 0xff, 0x80)
            waitForRendering(canvas3d) // this will pause until timeout, which is unavoidable

            // No render should happen since it has not been requested
            compare(renderOk, false);

            // Check color in the center of the colored square
            xpos = 150
            ypos = 150
            canvas3d.requestRender();
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x22)
            tryCompare(top, "green", 0x99)
            tryCompare(top, "blue", 0xff)
            tryCompare(top, "alpha", 0x80)

            // Check color in the corner of the screen
            xpos = 0
            ypos = 0
            renderOk = false
            canvas3d.requestRender();
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x40)
            tryCompare(top, "green", 0x50)
            tryCompare(top, "blue", 0x60)
            tryCompare(top, "alpha", 0xff)
        }

        // Check that resize triggers rendering
        function test_render_3_ondemand() {
            // In case this test case is run by itself, clear the initial render
            tryCompare(top, "renderOk", true)

            // Check width change
            Content.setColor(0x11, 0x22, 0x33, 0x44)
            xpos = 150
            ypos = 150
            renderOk = false
            canvas3d.width++
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x11)
            tryCompare(top, "green", 0x22)
            tryCompare(top, "blue", 0x33)
            tryCompare(top, "alpha", 0x44)

            // Check height change
            Content.setColor(0x33, 0x44, 0x55, 0x66)
            renderOk = false
            canvas3d.height++
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x33)
            tryCompare(top, "green", 0x44)
            tryCompare(top, "blue", 0x55)
            tryCompare(top, "alpha", 0x66)
        }
    }
}
