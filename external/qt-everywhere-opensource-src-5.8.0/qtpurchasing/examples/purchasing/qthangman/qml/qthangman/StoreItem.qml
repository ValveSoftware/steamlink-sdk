/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtPurchasing 1.0
import QtQuick.Controls 1.1

Rectangle {
    id: storeItem

    property Product product: undefined

    state: "NORMAL"

    visible: product.status == Product.Registered
    radius: 10
    color: "white"

    height: titleText.contentHeight + descriptionText.contentHeight + topLevel.globalMargin * 2
    // ![0]
    Text {
        id: titleText
        text: product.title
        font.bold: true
        anchors.right: priceText.left
        anchors.rightMargin: topLevel.globalMargin
        anchors.top: parent.top
        anchors.topMargin: topLevel.globalMargin
        anchors.left: parent.left
        anchors.leftMargin: topLevel.globalMargin
    }

    Text {
        id: descriptionText
        text: product.description
        anchors.right: priceText.left
        anchors.rightMargin: topLevel.globalMargin
        anchors.left: parent.left
        anchors.leftMargin: topLevel.globalMargin
        anchors.top: titleText.bottom
        anchors.topMargin: topLevel.globalMargin / 2
        wrapMode: Text.WordWrap
    }

    Text {
        id: priceText
        text: product.price
        anchors.right: parent.right
        anchors.rightMargin: topLevel.globalMargin
        anchors.verticalCenter: parent.verticalCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            pendingRect.visible = true;
            spinBox.visible = true;
            statusText.text = "Purchasing...";
            storeItem.state = "PURCHASING";
            product.purchase();
        }
        onPressed: {
            storeItem.state = "PRESSED";
        }
        onReleased: {
            storeItem.state = "NORMAL";
        }
    }
    // ![0]

    Rectangle {
        id: pendingRect
        anchors.fill: parent
        opacity: 0.0
        color: "white"
        radius: parent.radius
        Text {
            id: statusText
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: spinBox.left
            anchors.margins: topLevel.globalMargin
            verticalAlignment: Text.AlignVCenter
        }
        BusyIndicator {
            id: spinBox
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: topLevel.globalMargin
            width: height
        }

        Connections {
            target: product
            onPurchaseSucceeded: {
                statusText.text = "Purchase Succeeded";
                spinBox.visible = false;

            }
            onPurchaseFailed: {
                statusText.text = "Purchase Failed";
                spinBox.visible = false;
                storeItem.state = "NORMAL";
            }
        }
    }

    states: [
        State {
            name: "NORMAL"
            PropertyChanges {
                target: storeItem
                color: "white"
                border.color: "transparent"
            }
            PropertyChanges {
                target: pendingRect
                opacity: 0.0
            }
        },
        State {
            name: "PRESSED"
            PropertyChanges {
                target: storeItem
                color: "transparent"
                border.color: "white"
            }
        },
        State {
            name: "PURCHASING"
            PropertyChanges {
                target: pendingRect
                opacity: 1.0
            }
        }
    ]

    transitions: [
        Transition {
            from: "PURCHASING"
            to: "NORMAL"
            NumberAnimation { target: pendingRect; property: "opacity"; duration: 2000; easing.type: Easing.InExpo }
        }
    ]
}
