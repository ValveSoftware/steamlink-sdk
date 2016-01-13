/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

function printOrientation(orientation) {
    var orientationString;
    if (orientation == Orientation.Portrait) {
        orientationString = "Portrait";
    } else if (orientation == Orientation.Landscape) {
        orientationString = "Landscape";
    } else if (orientation == Orientation.PortraitInverted) {
        orientationString = "Portrait inverted";
    } else if (orientation == Orientation.LandscapeInverted) {
        orientationString = "Landscape inverted";
    } else {
        orientationString = "UnknownOrientation";
    }
    return orientationString;
}

function getAngle(orientation) {
    var angle;
    if (orientation == Orientation.Portrait) {
        angle = 0;
    } else if (orientation == Orientation.Landscape) {
        angle = 90;
    } else if (orientation == Orientation.PortraitInverted) {
        angle = 180;
    } else if (orientation == Orientation.LandscapeInverted) {
        angle = 270;
    } else {
        angle = 0;
    }
    return angle;
}

function parallel(firstOrientation, secondOrientation) {
    var difference = getAngle(firstOrientation) - getAngle(secondOrientation)
    return difference % 180 == 0;
}

function calculateGravityPoint(firstOrientation, secondOrientation) {
    var position = Qt.point(0, 0);
    var difference = getAngle(firstOrientation) - getAngle(secondOrientation)
    if (difference < 0) {
        difference = 360 + difference;
    }
    if (difference == 0) {
        position = Qt.point(0, -10);
    } else if (difference == 90) {
        position = Qt.point(-10, 0);
    } else if (difference == 180) {
        position = Qt.point(0, 1000);
    } else if (difference == 270) {
        position = Qt.point(1000, 0);
    }
    return position;
}
