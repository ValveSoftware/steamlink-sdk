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

import "tst_render_preserve.js" as Content

Item {
    id: top
    height: 300
    width: 300

    property int xpos: 0
    property int ypos: 0
    property var pixels
    property int red: -1
    property int green: -1
    property int blue: -1
    property int alpha: -1
    property bool paintDone: true
    property bool checkDone: true
    property bool initDone: false

    Canvas3D {
        id: render_preserve
        property bool textureLoaded: false
        anchors.fill: parent
        onInitializeGL: {
            Content.initializeGL(this)
            initDone = true
        }
        onPaintGL: {
            if (!paintDone) {
                Content.paintGL()
                paintDone = true
            }

            if (!checkDone) {
                pixels = Content.checkPixel(xpos, ypos)
                red = pixels[0]
                green = pixels[1]
                blue = pixels[2]
                alpha = pixels[3]
                checkDone = true
            }
        }
    }

    TestCase {
        name: "Canvas3D_render_preserve"
        when: windowShown

        function test_render_1_preserve() {
            tryCompare(top, "initDone", true)
            Content.setColor(0x00, 0x00, 0xff, 0xff)
            // Draw the frame exactly once. That way one of the framebuffers has our scene and
            // the other is default empty.
            paintDone = false;
            tryCompare(top, "paintDone", true)

            // We cannot control if we are reading from the first or second framebuffer, since we
            // cannot actually reliably sync with the render loop. We work around this issue
            // by doing the pixel checks multiple times. That way at least one of the checks
            // is likely to target the framebuffer we didn't draw our frame to, and therefore
            // fails if the preserve is not working.
            for (var i = 0; i < 10; i++) {
                // Check color in the left side (part of the triangle)
                xpos = 75
                ypos = 150
                checkDone = false
                tryCompare(top, "checkDone", true)
                compare(top.red, 0x00)
                compare(top.green, 0x00)
                compare(top.blue, 0xff)
                compare(top.alpha, 0xff)

                // Check color in the right part of the screen, which is cleared with red
                xpos = 225
                ypos = 150
                checkDone = false
                tryCompare(top, "checkDone", true)
                compare(top.red, 0xff)
                compare(top.green, 0x00)
                compare(top.blue, 0x00)
                compare(top.alpha, 0xff)
            }

            Content.setColor(0x00, 0xcc, 0x00, 0xff)
            paintDone = false;
            tryCompare(top, "paintDone", true)

            for (var j = 0; j < 10; j++) {
                // Check color in the left side (part of the triangle)
                xpos = 75
                ypos = 150
                checkDone = false
                tryCompare(top, "checkDone", true)
                compare(top.red, 0x00)
                compare(top.green, 0xcc)
                compare(top.blue, 0x00)
                compare(top.alpha, 0xff)
            }
        }
    }
}
