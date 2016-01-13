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
import QtQuick 2.0
import QtLocation 5.3

MapRectangle {
    id: mapRectangle

    color: mousearea.containsMouse ? "lime" : "red"
    opacity: 0.5
    border.width: 2.0

    function setGeometry(markers, index){
        topLeft.latitude = Math.max(markers[index].coordinate.latitude, markers[index + 1].coordinate.latitude)
        topLeft.longitude = Math.min(markers[index].coordinate.longitude, markers[index + 1].coordinate.longitude)
        bottomRight.latitude = Math.min(markers[index].coordinate.latitude, markers[index + 1].coordinate.latitude)
        bottomRight.longitude = Math.max(markers[index].coordinate.longitude, markers[index + 1].coordinate.longitude)
    }

    Binding {
        target: mapRectangle
        property: 'border.width'
        value: 60

        when: ((topLeft.latitude == -27.1144) &&
               (topLeft.longitude == 152.6594) &&
               (bottomRight.latitude == -27.7434) &&
               (bottomRight.longitude == 153.3021))
    }

    MouseArea {
        anchors.fill:parent
        id: mousearea
        hoverEnabled: false
        drag.target: parent
    }
}
