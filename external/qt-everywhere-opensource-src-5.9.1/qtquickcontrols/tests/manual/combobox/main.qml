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
import QtQuick.Controls.Styles 1.1
import QtQuick.Layouts 1.0
import QtQuick.Dialogs 1.0
import QtQuick.Window 2.1

ApplicationWindow {
    width: 640
    height: 480
    minimumWidth: 400
    minimumHeight: 300


    ListModel {
        id: listModel
        ListElement { text: "Norway"}
        ListElement { text: "North Korea"}
        ListElement { text: "Noruag"}
        ListElement { text: "Noman"}
        ListElement { text: "Nothing"}
        ListElement { text: "Nargl"}
        ListElement { text: "Argldaf"}
    }
    ColumnLayout {
        anchors.centerIn: parent
        RowLayout {
            Label { text: "validated model" ; Layout.fillWidth: true }
            ComboBox {
                editable: true
                validator: IntValidator { bottom: 0 ; top: 100 }
                model: [1, 2, 3, 4, 5]
            }
        }
        RowLayout {
            Label { text: "Array" ; Layout.fillWidth: true}
            ComboBox {
                width: 200
                model: ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
                style: ComboBoxStyle {}
            }
            ComboBox {
                width: 200
                editable: true
                model: ['Banana', 'Coco', 'Coconut', 'Apple', 'Cocomuffin' ]
                style: ComboBoxStyle {}
            }
        }
        RowLayout {
            Label { text: "StandardItemModel" ; Layout.fillWidth: true}
            ComboBox {
                width: 200
                textRole: "display"
                model: standardmodel
                style: ComboBoxStyle {}
            }
            ComboBox {
                width: 200
                textRole: "display"
                editable: true
                model: standardmodel
                style: ComboBoxStyle {}
            }
        }
        RowLayout {
            Label { text: "ListModel" ; Layout.fillWidth: true}
            ComboBox {
                width: 200
                textRole: "text"
                model: listModel
            }
            ComboBox {
                width: 200
                textRole: "text"
                editable: true
                model: listModel
            }
        }
        RowLayout {
            Label { text: "QStringList" ; Layout.fillWidth: true}
            ComboBox {
                width: 200
                model: stringmodel
            }
            ComboBox {
                width: 200
                editable: true
                model: stringmodel
            }
        }
    }
}
