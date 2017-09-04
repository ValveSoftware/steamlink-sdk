/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

import QtQuick 2.0
import QtTest 1.1

Item {
    id: top

    Rectangle { id: empty }

    Rectangle { id: radius }

    Rectangle {
        id: resized
        width: 300
        height: 300
    }

    Rectangle {
        id: smooth
        smooth: true
        width: 300
        height: 300
    }

    Rectangle {
        id: gradient
        width: 100
        height: 300
        gradient: Gradient {
            GradientStop { position: 0.0; color: "red" }
            GradientStop { position: 0.5; color: "yellow" }
            GradientStop { position: 1.0; color: "green" }
        }
    }

    Rectangle {
        id: rectangleborder
        width: 300
        height: 150
        border.width: 1
        border.color: "gray"
    }

    TestCase {
        name: "Rectangle"

        function test_empty() {
            compare(empty.width, 0)
            compare(empty.height, 0)
        }

        function test_radius() {
            compare(radius.width, 0)
            compare(radius.height, 0)
            compare(radius.radius, 0)
            radius.height = 100;
            radius.width = 100;
            radius.radius = 10;
            compare(radius.width, 100)
            compare(radius.height, 100)
            compare(radius.radius, 10)
        }

        function test_resized() {
            compare(resized.width, 300)
            compare(resized.height, 300)
            resized.height = 500;
            resized.width = 500;
            compare(resized.width, 500)
            compare(resized.height, 500)
        }

        function test_smooth() {
            compare(smooth.smooth, true)
            compare(smooth.width, 300)
            compare(smooth.height, 300)

        }

        function test_gradient() {
            var grad = gradient.gradient;
            var gstops = grad.stops;
            compare(gstops[0].color.toString(), "#ff0000")
            compare(gstops[1].color.toString(), "#ffff00")
            compare(gstops[2].color.toString(), "#008000")
        }

        function test_borders() {
            compare(rectangleborder.border.width, 1)
            compare(rectangleborder.border.color.toString(), "#808080")
            rectangleborder.border.width = 10;
            rectangleborder.border.color = "brown";
            compare(rectangleborder.border.width, 10)
            compare(rectangleborder.border.color.toString(), "#a52a2a")
        }

    }
}
