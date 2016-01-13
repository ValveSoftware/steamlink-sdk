/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 1.0

Item {
    id: main
    width: 360
    height: 640

    Rectangle {
        id: background
        visible: testCaseList.visible
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#EEEEEE" }
                 GradientStop { position: 1.0; color: "#AAAAAA" }
             }
    }

    Loader {
        id: testLoader
        width: parent.width
        height: parent.height
        visible: !testCaseList.visible
    }

    ListModel {
        id: testcaseModel
        ListElement { name: "TestEffectHierarchy.qml"; group: "Effect source property tests" }
        ListElement { name: "TestGrab.qml"; group: "Effect source property tests" }
        ListElement { name: "TestLive.qml"; group: "Effect source property tests" }
        ListElement { name: "TestImageFiltering.qml"; group: "Effect source property tests" }
        ListElement { name: "TestWrapRepeat.qml"; group: "Effect source property tests" }
        ListElement { name: "TestHorizontalWrap.qml"; group: "Effect source property tests" }
        ListElement { name: "TestVerticalWrap.qml"; group: "Effect source property tests" }
        ListElement { name: "TestTextureSize.qml"; group: "Effect source property tests" }
        ListElement { name: "TestItemMargins.qml"; group: "Effect source property tests" }
        ListElement { name: "TestEffectInsideAnotherEffect.qml"; group: "Effect source property tests" }
        ListElement { name: "TestItemMarginsWithTextureSize.qml"; group: "Effect source property tests" }
        ListElement { name: "TestHideOriginal.qml"; group: "Effect source property tests" }
        ListElement { name: "TestActive.qml"; group: "Effect item property tests" }
        ListElement { name: "TestBlending.qml"; group: "Effect item property tests" }
        ListElement { name: "TestBlendingModes.qml"; group: "Effect item property tests" }
        ListElement { name: "TestOpacity.qml"; group: "Effect item property tests" }
        ListElement { name: "TestFragmentShader.qml"; group: "Effect item property tests" }
        ListElement { name: "TestVertexShader.qml"; group: "Effect item property tests" }
        ListElement { name: "TestMeshResolution.qml"; group: "Effect item property tests" }
        ListElement { name: "TestRotation.qml"; group: "Shader effect transformation tests" }
        ListElement { name: "TestScale.qml"; group: "Shader effect transformation tests" }
        ListElement { name: "TestBasic.qml"; group: "Scenegraph effect tests" }
        ListElement { name: "TestOneSource.qml"; group: "Scenegraph effect tests" }
        ListElement { name: "TestTwiceOnSameSource.qml"; group: "Scenegraph effect tests" }
        ListElement { name: "TestTwoSources.qml"; group: "Scenegraph effect tests" }
    }

    Component {
        id: sectionHeading
        Rectangle {
            width: testCaseList.width
            height: 35
            color: "#00000000"

            Text {
                text: section
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                anchors.fill: parent
                anchors.leftMargin: 5
                font.bold: true
                style: Text.Raised
                styleColor: "white"
            }
        }
    }

    ListView {
        id: testCaseList

        property int hideTranslation: 0
        transform: Translate {
            x: testCaseList.hideTranslation
        }

        anchors.fill: parent
        anchors.topMargin: 10
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 10

        model: testcaseModel
        spacing: 3

        state: "testStopped"

        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading

        delegate: Rectangle {
            width: parent.width
            height:  50
            radius:  5
            border.width: 1
            border.color:  "#888888"
            color: delegateMouseArea.pressed ? "#AAAAFF" : "#FFFFFF"
            Text {
                id: delegateText;
                text: "  " + name
                width: parent.width
                height: parent.height
                font.pixelSize: 16
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: delegateText2;
                text: ">  "
                width: parent.width
                height: parent.height
                font.pixelSize: 20
                smooth: true
                color: "gray"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
            }

            MouseArea {
                id: delegateMouseArea
                anchors.fill: parent;
                onClicked: {
                    testCaseList.state = "testRunning"
                    testLoader.source = name
                    console.log(name)
                }
            }
        }

        states: [
            State {
                name: "testRunning"
                PropertyChanges { target: testCaseList; visible: false; hideTranslation: -main.width  }
            },
            State {
                name: "testStopped"
                PropertyChanges { target: testCaseList; visible: true; hideTranslation: 0 }
            }
        ]

        transitions: [
            Transition {
            to: "testRunning"
                SequentialAnimation {
                    NumberAnimation { properties: "hideTranslation"; easing.type: Easing.InQuad; duration:  300 }
                    PropertyAction { target: testCaseList; property: "visible"; value: false }
                }
             },
            Transition {
                to: "testStopped"
                SequentialAnimation {
                    PropertyAction { target: testCaseList; property: "visible"; value: true }
                    NumberAnimation { properties: "hideTranslation"; easing.type: Easing.InQuad; duration:  300 }
                }
            }

        ]
    }

    Rectangle {
        visible: true
        anchors.bottom: main.bottom
        anchors.left: main.left
        anchors.right: main.right
        height: 40
        color: "#cc000000"
        Item {
            anchors.top: parent.top
            anchors.topMargin: 5
            anchors.left: parent.left
            anchors.leftMargin: 20
            Image {
                source: "back.svg"
            }
        }

        MouseArea {
            anchors.fill: parent;
            onClicked: {
                if (testCaseList.visible){
                    Qt.quit()
                } else if (!testCaseList.state != "testStopped") {
                    testCaseList.state = "testStopped"
                    testLoader.source = ""
                }
            }
        }
    }
}

