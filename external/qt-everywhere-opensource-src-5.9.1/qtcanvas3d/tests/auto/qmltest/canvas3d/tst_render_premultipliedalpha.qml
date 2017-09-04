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

import QtQuick 2.6
import QtCanvas3D 1.0
import QtTest 1.0

import "tst_render_premultipliedalpha.js" as Content

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
    property bool checkDone: false
    property bool initDone: false
    property bool paintDone: false

    Rectangle {
        id: canvasRect
        anchors.fill: parent
        color: "#FF00FF"

        Canvas3D {
            id: canvas
            property bool textureLoaded: false
            anchors.fill: parent
            onInitializeGL: {
                Content.initializeGL(this)
                initDone = true
            }
            onPaintGL: {
                Content.paintGL()
                paintDone = true

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
    }

    TestCase {
        name: "Canvas3D_render_premultiplyalpha"
        when: windowShown

        // This test checks that premultiplying alpha render step works for contexts that specify
        // premultipliedAlpha:false.
        function test_render_1_premultiplyAlpha() {
            tryCompare(top, "initDone", true)

            Content.setColor(0x00, 0x00, 0xff, 0xff)
            waitForRendering(canvas)
            tryCompare(top, "paintDone", true)

            // Check color in the left side (part of the triangle)
            xpos = 75
            ypos = 150
            checkDone = false
            tryCompare(top, "checkDone", true)
            compare(top.red, 0x00)
            compare(top.green, 0x00)
            compare(top.blue, 0xff)
            compare(top.alpha, 0xff)

            // Check color in the right part of the screen, which shows the background rectangle
            // The result of gl.readPixels should return the premultiplied values for colors,
            // i.e. 0

            xpos = 225
            ypos = 150
            checkDone = false
            tryCompare(top, "checkDone", true)
            compare(top.red, 0x00)
            compare(top.green, 0x00)
            compare(top.blue, 0x00)
            compare(top.alpha, 0x00)

            // Grabbing an image and checking final colors should show the background colors
            var image = grabImage(canvasRect);
            verify(image.pixel(xpos, ypos) === Qt.rgba(1, 0, 1, 1));
        }
    }
}
