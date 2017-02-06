/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

import QtQuick 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 1.4
import Qt3D.Render 2.1
import QtQuick.Scene3D 2.0

Item {

    width: 1250
    height: 700

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10

        Rectangle {
            id: background
            width: 600
            height: 600

            color: "blue"

            Scene3D {
                id: scene3d
                anchors.fill: parent
                multisample: msacheckbox.checked

                aspects: ["input", "logic"]

                CaptureScene {
                    id: scene
                }
            }
        }

        ColumnLayout {

            Button {
                id: button
                anchors.top: parent.top
                text: "Render Capture"

                property var reply
                property bool continuous : checkbox.checked
                property int cid: 1

                function doRenderCapture()
                {
                    reply = scene.requestRenderCapture(cid)
                    reply.completeChanged.connect(onRenderCaptureComplete)
                }

                function onRenderCaptureComplete()
                {
                    _renderCaptureProvider.updateImage(reply)
                    image.source = "image://rendercapture/" + cid
                    reply.saveToFile("capture" + cid + ".png")
                    cid++
                    if (continuous === true)
                        doRenderCapture()
                }

                onClicked: doRenderCapture()
            }
            RowLayout {
                CheckBox {
                    id: checkbox
                    text: "continuous"
                }
                CheckBox {
                    id: msacheckbox
                    text: "multisample"
                }
            }
            Image {
                id: image
                cache: false
                source: "image://rendercapture/0"
                Layout.maximumWidth: 600
                Layout.minimumWidth: 600
                Layout.maximumHeight: 600
                Layout.minimumHeight: 600
            }
        }
    }
}
