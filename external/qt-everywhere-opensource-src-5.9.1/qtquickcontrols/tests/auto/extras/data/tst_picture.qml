/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

import QtTest 1.0
import QtQuick 2.4
import QtQuick.Extras 1.4
import QtQuick.Extras.Private 1.0

TestCase {
    id: testCase
    name: "Tests_Picture"
    visible: true
    when: windowShown
    width: 400
    height: 400

    Rectangle {
        anchors.fill: parent
        color: "white"
    }

    property var picture

    /*
        picture.dat was generated with a 10 width pen, and:

        painter.drawEllipse(painter.pen().width() / 2, painter.pen().width() / 2,
            100 - painter.pen().width(), 100 - painter.pen().width());
    */
    property color pictureDotDatDefaultColor: Qt.rgba(0, 0, 0, 1)
    property size pictureDotDatImplicitSize: Qt.size(100, 100)

    function cleanup() {
        if (picture)
            picture.destroy();
    }

    function test_instance() {
        picture = Qt.createQmlObject("import QtQuick.Extras 1.4; Picture { }", testCase, "");
        verify(picture, "Picture: failed to create an instance");
    }

    function verifyColorEqualMessage(pictureImage, pixelX, pixelY, expectedPixelColor) {
        return "pixel " + pictureImage.pixel(pixelX, pixelY) + " at "
            + pixelX + "," + pixelY + " isn't equal to " + expectedPixelColor;
    }

    function test_source_data() {
        return [
            {
                tag: "picture.dat",
                implicitSize: pictureDotDatImplicitSize,
                pixels: [
                    { x: 0, y: 0, color: Qt.rgba(1, 1, 1, 1) },
                    { x: 18, y: 18, color: pictureDotDatDefaultColor },
                    { x: 50, y: 50, color: Qt.rgba(1, 1, 1, 1) }
                ]
            }
        ];
    }

    function test_source(data) {
        picture = Qt.createQmlObject("import QtQuick.Extras 1.4; Picture {}", testCase, "");
        verify(picture, "Picture: failed to create an instance");
        picture.source = data.tag;
        picture.width = data.implicitSize.width;
        picture.height = data.implicitSize.height;
        waitForRendering(picture);

        var pictureImage = grabImage(picture);

        for (var i = 0; i < data.pixels.length; ++i) {
            var pixel = data.pixels[i];
            // TODO: use compare when QTBUG-34878 is fixed
            verify(Qt.colorEqual(pictureImage.pixel(pixel.x, pixel.y), pixel.color),
                verifyColorEqualMessage(pictureImage, pixel.x, pixel.y, pixel.color));
        }
    }

    function test_color_data() {
        return [
            { tag: "undefined", color: undefined, expectedColor: pictureDotDatDefaultColor },
            { tag: "not a valid color", color: "not a valid color", expectedColor: pictureDotDatDefaultColor },
            { tag: "red", color: "red", expectedColor: Qt.rgba(1, 0, 0, 1) },
            { tag: "black", color: "black", expectedColor: Qt.rgba(0, 0, 0, 1) }
        ]
    }

    function test_color(data) {
        picture = Qt.createQmlObject("import QtQuick.Extras 1.4; Picture {}", testCase, "");
        verify(picture, "Picture: failed to create an instance");

        picture.width = pictureDotDatImplicitSize.width;
        picture.height = pictureDotDatImplicitSize.height;
        picture.source = "picture.dat";
        picture.color = data.color;
        // For some reason we need two waits here, otherwise the color detected is *sometimes* white instead of black.
        // Also, we use an explicit, shorter timeout, otherwise the default (5000 ms) seems to be exhausted.
        waitForRendering(picture, 200);
        waitForRendering(picture, 200);

        var pictureImage = grabImage(picture);

        verify(Qt.colorEqual(pictureImage.pixel(0, 0), Qt.rgba(1, 1, 1, 1)),
               verifyColorEqualMessage(pictureImage, 0, 0, Qt.rgba(1, 1, 1, 1)));
        verify(Qt.colorEqual(pictureImage.pixel(17, 17), data.expectedColor),
               verifyColorEqualMessage(pictureImage, 17, 17, data.expectedColor));
        verify(Qt.colorEqual(pictureImage.pixel(picture.width / 2, picture.height / 2), Qt.rgba(1, 1, 1, 1)),
               verifyColorEqualMessage(pictureImage, picture.width / 2, picture.height / 2, Qt.rgba(1, 1, 1, 1)));
    }

    FontMetrics {
        id: fontMetrics
    }

    function test_size() {
        picture = Qt.createQmlObject("import QtQuick.Extras 1.4; Picture {}", testCase, "");
        verify(picture, "Picture: failed to create an instance");

        compare(picture.implicitWidth, fontMetrics.height * 4);
        compare(picture.implicitHeight, fontMetrics.height * 4);
        compare(picture.width, picture.implicitWidth);
        compare(picture.height, picture.implicitHeight);

        picture.source = "picture.dat";
        compare(picture.implicitWidth, pictureDotDatImplicitSize.width);
        compare(picture.implicitHeight, pictureDotDatImplicitSize.height);
        compare(picture.width, picture.implicitWidth);
        compare(picture.height, picture.implicitHeight);
    }
}
