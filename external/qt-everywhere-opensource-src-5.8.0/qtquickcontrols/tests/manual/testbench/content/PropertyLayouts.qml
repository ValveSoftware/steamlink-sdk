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

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.0

QtObject {
    property Component boolLayout: CheckBox {
        checked: visible ? (result == "true") : false
        text: name
        onCheckedChanged: {
            if (!ignoreUpdate) {
                loader.item[name] = checked
                propertyChanged()
            }
        }
    }

    property Component intLayout: RowLayout {
        spacing: 4
        Label {
            text: name + ":"
            Layout.minimumWidth: 100
        }
        SpinBox {
            value: result
            maximumValue: 9999
            minimumValue: -9999
            Layout.fillWidth: true
            onValueChanged: {
                if (!ignoreUpdate) {
                    loader.item[name] = value
                    propertyChanged()
                }
            }
        }
    }

    property Component realLayout: RowLayout {
        spacing: 4
        Label {
            text: name + ":"
            Layout.minimumWidth: 100
        }
        SpinBox {
            id: spinbox
            value: result
            decimals: 1
            stepSize: 0.5
            maximumValue: 9999
            minimumValue: -9999
            Layout.fillWidth: true
            onValueChanged: {
                if (!ignoreUpdate) {
                    loader.item[name] = value
                    if (name != "width" && name != "height") // We don't want to reset size when size changes
                        propertyChanged()
                }
            }

            Component.onCompleted: {
                if (name == "width")
                    widthControl = spinbox
                else if (name == "height")
                    heightControl = spinbox
            }
        }
    }

    property Component stringLayout: RowLayout {
        spacing: 4
        Label {
            text: name + ":"
            width: 100
        }
        TextField {
            id: tf
            text: result
            Layout.fillWidth: true
            onTextChanged: {
                if (!ignoreUpdate) {
                    loader.item[name] = tf.text
                    propertyChanged()
                }
            }
        }
    }

    property Component readonlyLayout: RowLayout {
        height: 20
        Label {
            id: text
            height: 20
            text: name + ":"
        }
        Label {
            height: 20
            anchors.right: parent.right
            Layout.fillWidth: true
            text: loader.item[name] !== undefined ? loader.item[name] : ""
        }
    }

    property Component enumLayout: Column {
        id: enumLayout
        spacing: 4

        Label { text: name + ":" }

        ComboBox {
            height: 20
            width: parent.width
            model: enumModel
            onCurrentIndexChanged: {
                if (!ignoreUpdate) {
                    loader.item[name] = model.get(currentIndex).value
                    propertyChanged()
                }
            }
            Component.onCompleted: currentIndex = getDefaultIndex()
            function getDefaultIndex() {
                for (var index = 0 ; index < model.count ; ++index) {
                    if ( model.get(index).value === result )
                        return index
                }
                return 0;
            }

        }
    }
}
