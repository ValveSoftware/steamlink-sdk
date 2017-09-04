/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.3
import QtQuick.Window 2.1
import "../shared" as Shared

Item {
    id: root
    width: 400
    height: propertyGrid.implicitHeight + 16

    function orientationToString(o) {
        switch (o) {
        case Qt.PrimaryOrientation:
            return "primary";
        case Qt.PortraitOrientation:
            return "portrait";
        case Qt.LandscapeOrientation:
            return "landscape";
        case Qt.InvertedPortraitOrientation:
            return "inverted portrait";
        case Qt.InvertedLandscapeOrientation:
            return "inverted landscape";
        }
        return "unknown";
    }

    Grid {
        id: propertyGrid
        columns: 2
        spacing: 8
        x: spacing
        y: spacing

        //! [screen]
        Shared.Label {
            text: "Screen \"" + Screen.name + "\":"
            font.bold: true
        }
        Item { width: 1; height: 1 } // spacer

        Shared.Label { text: "dimensions" }
        Shared.Label { text: Screen.width + "x" + Screen.height }

        Shared.Label { text: "pixel density" }
        Shared.Label { text: Screen.pixelDensity.toFixed(2) + " dots/mm (" + (Screen.pixelDensity * 25.4).toFixed(2) + " dots/inch)" }

        Shared.Label { text: "logical pixel density" }
        Shared.Label { text: Screen.logicalPixelDensity.toFixed(2) + " dots/mm (" + (Screen.logicalPixelDensity * 25.4).toFixed(2) + " dots/inch)" }

        Shared.Label { text: "device pixel ratio" }
        Shared.Label { text: Screen.devicePixelRatio.toFixed(2) }

        Shared.Label { text: "available virtual desktop" }
        Shared.Label { text: Screen.desktopAvailableWidth + "x" + Screen.desktopAvailableHeight }

        Shared.Label { text: "position in virtual desktop" }
        Shared.Label { text: Screen.virtualX + ", " + Screen.virtualY }

        Shared.Label { text: "orientation" }
        Shared.Label { text: orientationToString(Screen.orientation) + " (" + Screen.orientation + ")" }

        Shared.Label { text: "primary orientation" }
        Shared.Label { text: orientationToString(Screen.primaryOrientation) + " (" + Screen.primaryOrientation + ")" }
        //! [screen]

        Shared.Label { text: "10mm rectangle" }
        Rectangle {
            color: "red"
            width: Screen.pixelDensity * 10
            height: width
        }
    }
}
