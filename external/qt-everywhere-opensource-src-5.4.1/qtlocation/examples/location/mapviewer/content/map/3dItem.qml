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
import QtQuick 2.0;
import QtLocation 5.3
import QtQuick 2.0
import Qt3D 2.0
import Qt3D.Shapes 2.0

// NOTE: If the creation of this compoment fails, make sure you have the latest
// qtquick3d (qml2 branch) and qtdeclarative installed.
// You can test your system with qtquick3d examples.
MapQuickItem {  //to be used inside MapComponent only
    id: mapItem

    //    zoomLevel: 8 // set this if you want the item to follow zoom level changes

    function setGeometry(markers, index) {
        coordinate.latitude = markers[index].coordinate.latitude
        coordinate.longitude = markers[index].coordinate.longitude
    }

    anchorPoint.x: testItem.width/2
    anchorPoint.y: testItem.height/2

    sourceItem: Item {
        id: testItem
        width: 100
        height: 100
        Viewport {
            width: parent.width; height: parent.height
            renderMode: "BufferedRender"
            navigation: false // set this true if you want to rotate/zoom with mouse
                              // but that interferes with map panning on 3d item area
            Cube {
                scale: 2
                transform: [
                    Rotation3D {
                        angle: 45
                        axis: Qt.vector3d(1, 0, 1)
                    },
                    Rotation3D {
                        id: spin
                        angle: 0
                        axis: Qt.vector3d(0, 1, 0)
                    }]

                NumberAnimation {
                    target: spin
                    property: "angle"
                    from: 0
                    to: 360; duration: 5000; running: true
                    loops: NumberAnimation.Infinite }

                effect: Effect {
                    color: "#aaca00"
                    texture: "../../icon.png"
                    decal: true
                }
            }
        }
    }
}
