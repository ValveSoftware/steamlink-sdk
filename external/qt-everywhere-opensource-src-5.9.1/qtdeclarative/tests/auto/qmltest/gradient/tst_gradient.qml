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

    Gradient {
        id: emptygradient
    }

    Gradient {
        id: twogradients
        GradientStop { position: 0.0; color: "red" }
        GradientStop { position: 1.0; color: "green" }
    }

    Gradient {
        id: elevengradients
        GradientStop { position: 0.0; color: "red" }
        GradientStop { position: 0.1; color: "orange" }
        GradientStop { position: 0.2; color: "yellow" }
        GradientStop { position: 0.3; color: "green" }
        GradientStop { position: 0.4; color: "blue" }
        GradientStop { position: 0.5; color: "violet" }
        GradientStop { position: 0.6; color: "indigo" }
        GradientStop { position: 0.7; color: "brown" }
        GradientStop { position: 0.8; color: "lightgray" }
        GradientStop { position: 0.9; color: "gray" }
        GradientStop { position: 1.0; color: "black" }
    }

    Gradient {
        id: movedgradients
        property real stopposition: 0.5
        GradientStop { position: 0.0; color: "red" }
        GradientStop { position: movedgradients.stopposition; color: "blue" }
        GradientStop { position: 1.0; color: "green" }
    }

    Gradient {
        id: defaultgradient
        GradientStop { }
        GradientStop { position: 1.0; color: "red" }
    }

    TestCase {
        name: "Gradient"

        function test_empty() {
            compare(emptygradient.stops.length, 0)
        }

        function test_lengthtwo() {
            compare(twogradients.stops.length, 2)
            compare(twogradients.stops[0].color.toString(), "#ff0000")
            compare(twogradients.stops[1].color.toString(), "#008000")
        }

        function test_multiplestops() {
            compare(elevengradients.stops.length, 11)
            compare(elevengradients.stops[0].color.toString(), "#ff0000")
            compare(elevengradients.stops[4].color.toString(), "#0000ff")
            compare(elevengradients.stops[4].position, 0.4)
            compare(elevengradients.stops[9].position, 0.9)
        }

        function test_moved() {
            compare(movedgradients.stops.length, 3)
            compare(movedgradients.stops[1].position, 0.5)
            movedgradients.stopposition = 0.3;
            compare(movedgradients.stops[1].position, 0.3)
        }

        function test_default() {
            compare(defaultgradient.stops.length, 2)
            compare(defaultgradient.stops[0].color.toString(), "#000000")
        }
    }
}
