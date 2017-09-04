/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2

Item {
    id: content

    width: 480
    height: 480

    property alias address: address
    property alias gridLayout: gridLayout
    property alias cancel: cancel
    property alias save: save

    property alias title: title
    property alias zipCode: zipCode
    property alias city: city
    property alias phoneNumber: phoneNumber
    property alias customerId: customerId
    property alias email: email
    property alias lastName: lastName
    property alias firstName: firstName

    GridLayout {
        id: gridLayout

        anchors.right: parent.right
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.rightMargin: 12
        anchors.leftMargin: 12
        anchors.topMargin: 12
        columnSpacing: 8
        rowSpacing: 8
        rows: 8
        columns: 7
        enabled: false

        Label {
            id: label1
            text: qsTr("Title")
            Layout.columnSpan: 2
        }

        Label {
            id: label2
            text: qsTr("First Name")
            Layout.columnSpan: 2
        }

        Item {
            id: spacer10
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        Label {
            id: label3
            text: qsTr("Last Name")
        }

        Item {
            id: spacer15
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        ComboBox {
            id: title
            Layout.columnSpan: 2
            Layout.fillWidth: true
            model: ["Mr.", "Ms."]
        }

        TextField {
            id: firstName
            Layout.minimumWidth: 140
            Layout.fillWidth: true
            Layout.columnSpan: 3
            placeholderText: qsTr("first name")
        }

        TextField {
            id: lastName
            Layout.minimumWidth: 140
            Layout.fillWidth: true
            Layout.columnSpan: 2
            placeholderText: qsTr("last name")
        }

        Label {
            id: label4
            text: qsTr("Phone Number")
            Layout.columnSpan: 5
        }

        Label {
            id: label5
            text: qsTr("Email")
            Layout.preferredHeight: 13
            Layout.preferredWidth: 24
        }

        Item {
            id: spacer16
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        TextField {
            id: phoneNumber
            Layout.fillWidth: true
            Layout.columnSpan: 5
            placeholderText: qsTr("phone number")
        }

        TextField {
            id: email
            Layout.fillWidth: true
            Layout.columnSpan: 2
            placeholderText: qsTr("email")
        }

        Label {
            id: label6
            text: qsTr("Address")
        }

        Item {
            id: spacer3
            Layout.columnSpan: 6
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        TextField {
            id: address
            Layout.fillWidth: true
            Layout.columnSpan: 7
            placeholderText: qsTr("address")
        }

        Label {
            id: label7
            text: qsTr("City")
        }

        Item {
            id: spacer4
            Layout.columnSpan: 4
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        Label {
            id: label8
            text: qsTr("Zip Code")
        }

        Item {
            id: spacer18
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        TextField {
            id: city
            Layout.fillWidth: true
            Layout.columnSpan: 5
            placeholderText: qsTr("city")
        }

        TextField {
            id: zipCode
            Layout.fillWidth: true
            Layout.columnSpan: 2
            placeholderText: qsTr("zip code")
        }

        Label {
            id: label9
            text: qsTr("Customer Id")
        }

        Item {
            id: spacer19
            Layout.columnSpan: 6
            Layout.preferredHeight: 14
            Layout.preferredWidth: 14
        }

        TextField {
            id: customerId
            Layout.columnSpan: 7
            Layout.fillWidth: true
            placeholderText: qsTr("id")
        }
    }

    RowLayout {
        anchors.topMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: gridLayout.bottom

        Button {
            id: save
            text: qsTr("Save")
            enabled: false
        }

        Button {
            id: cancel
            text: qsTr("Cancel")
            enabled: false
        }
    }
}
