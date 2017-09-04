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
import QtCanvas3D 1.0
import QtTest 1.0

import "tst_render_checkresult.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property bool checkResult: false
    property int xpos: 0
    property int ypos: 0
    property var pixels
    property int red: -1
    property int green: -1
    property int blue: -1
    property int alpha: -1
    property bool renderOk: false
    property bool separateFbo: false

    Canvas3D {
        id: render_check_result
        property bool textureLoaded: false
        anchors.fill: parent
        onInitializeGL: Content.initializeGL(this)
        onPaintGL: {
            if (checkResult) {
                pixels = Content.paintGL(xpos, ypos, separateFbo)
                red = pixels[0]
                green = pixels[1]
                blue = pixels[2]
                alpha = pixels[3]
            } else {
                Content.paintGL()
                red = -1
                green = -1
                blue = -1
                alpha = -1
            }
            renderOk = true
        }
    }

    TestCase {
        name: "Canvas3D_render_checkresult"
        when: windowShown

        function test_render_1_checkresult() {
            // Check color in the center of the colored square
            xpos = 150
            ypos = 150
            checkResult = true
            renderOk = false
            separateFbo = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x10)
            tryCompare(top, "green", 0x20)
            tryCompare(top, "blue", 0x30)
            tryCompare(top, "alpha", 0xff)
            checkResult = false

            waitForRendering(render_check_result)

            // Check color in the corner of the screen
            xpos = 0
            ypos = 0
            checkResult = true
            renderOk = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x40)
            tryCompare(top, "green", 0x50)
            tryCompare(top, "blue", 0x60)
            tryCompare(top, "alpha", 0xff)
            checkResult = false

            waitForRendering(render_check_result)
        }

        function test_render_2_checkresult() {
            // Set a solid color texture, and check that the color matches in the center
            waitForRendering(render_check_result)
            Content.setTexture(render_check_result, "tst_render_checkresult.png")
            xpos = 150
            ypos = 150
            checkResult = true
            renderOk = false
            separateFbo = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(render_check_result, "textureLoaded", true, 10000)
            tryCompare(top, "red", 0xff)
            tryCompare(top, "green", 0x99)
            tryCompare(top, "blue", 0x22)
            tryCompare(top, "alpha", 0xff)
            checkResult = false
            Content.setTexture()

            waitForRendering(render_check_result)
        }

        function test_render_3_checkresult() {
            // Set a partially transparent color, and check it
            waitForRendering(render_check_result)
            Content.setColor(0x22, 0x99, 0xff, 0x80)
            xpos = 150
            ypos = 150
            checkResult = true
            renderOk = false
            separateFbo = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x22)
            tryCompare(top, "green", 0x99)
            tryCompare(top, "blue", 0xff)
            tryCompare(top, "alpha", 0x80)
            checkResult = false

            waitForRendering(render_check_result)
        }

        function test_render_4_checkresult() {
            // Render to separate FBO.
            waitForRendering(render_check_result)
            Content.setColor(0x40, 0x60, 0x80, 0xff)

            // Check color in the center of the square
            xpos = 150
            ypos = 150
            checkResult = true
            renderOk = false
            separateFbo = true
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x40)
            tryCompare(top, "green", 0x60)
            tryCompare(top, "blue", 0x80)
            tryCompare(top, "alpha", 0xff)
            checkResult = false

            waitForRendering(render_check_result)

            // Check color in the corner of the screen, which is cleared with green
            xpos = 0
            ypos = 0
            checkResult = true
            renderOk = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x00)
            tryCompare(top, "green", 0xff)
            tryCompare(top, "blue", 0x00)
            tryCompare(top, "alpha", 0xff)
            checkResult = false

            waitForRendering(render_check_result)
        }

        function test_render_5_checkresult() {
            // Check that pixels outside framebuffer are zero
            waitForRendering(render_check_result)
            Content.setColor(0x22, 0x99, 0xff, 0x80)
            xpos = 9999
            ypos = 9999
            checkResult = true
            renderOk = false
            separateFbo = false
            waitForRendering(render_check_result)
            tryCompare(top, "renderOk", true)
            tryCompare(top, "red", 0x0)
            tryCompare(top, "green", 0x0)
            tryCompare(top, "blue", 0x0)
            tryCompare(top, "alpha", 0x0)
            checkResult = false

            waitForRendering(render_check_result)
        }
    }
}
