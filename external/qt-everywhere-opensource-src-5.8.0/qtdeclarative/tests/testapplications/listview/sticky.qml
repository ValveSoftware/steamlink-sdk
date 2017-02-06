/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 2.4
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.1
import QtQuick.Controls.Styles 1.1
import Qt.labs.settings 1.0

ApplicationWindow {
    id: window

    width: 640
    height: 480
    visible: true

    property int hspacing: 10
    property int vspacing: 35

    Settings {
        property alias windowX: window.x
        property alias windowY: window.y
        property alias windowWidth: window.width
        property alias windowHeight: window.height
        property alias orientation: oriItem.currentIndex
        property alias layoutDirection: ldItem.currentIndex
        property alias verticalLayoutDirection: vldItem.currentIndex
        property alias hasHeader: hItem.checked
        property alias headerPositioning: shItem.currentIndex
        property alias hasFooter: fItem.checked
        property alias footerPositioning: sfItem.currentIndex
        property alias clipEnabled: clipItem.checked
        property alias filterEnabled: filterItem.checked
        property alias opacityEnabled: opacityItem.checked
        property alias inlineSections: isItem.checked
        property alias stickyCurrentSection: scsItem.checked
        property alias stickyNextSection: snsItem.checked
    }

    toolBar: GridLayout {
        rows: 3
        flow: GridLayout.TopToBottom

        anchors.margins: hspacing
        anchors.left: parent.left
        anchors.right: parent.right

        ComboBox { id: oriItem; currentIndex: 1; model: ["Horizontal", "Vertical"] }
        ComboBox { id: ldItem; model: ["LeftToRight", "RightToLeft"] }
        ComboBox { id: vldItem;  model: ["TopToBottom", "BottomToTop"] }

        CheckBox { id: clipItem; text: "Clip"; checked: true }
        CheckBox { id: opacityItem; text: "Opaque"; checked: true }
        CheckBox { id: filterItem; text: "Filter" }

        CheckBox { id: hItem; text: "Header:" }
        CheckBox { id: fItem; text: "Footer:" }
        Item { width: 1; height: 1 }

        ComboBox { id: shItem; model: shModel; textRole: "name"; enabled: hItem.checked }
        ComboBox { id: sfItem; model: sfModel; textRole: "name"; enabled: fItem.checked }
        Item { width: 1; height: 1 }

        CheckBox { id: scsItem; text: "Sticky current section" }
        CheckBox { id: snsItem; text: "Sticky next section" }
        CheckBox { id: isItem; text: "Inline sections" }

        Button { text: "Beginning"; onClicked: listview.positionViewAtBeginning() }
        Button { text: "Middle";  onClicked: listview.positionViewAtIndex(50, ListView.Center) }
        Button { text: "End"; onClicked: listview.positionViewAtEnd() }
    }

    ListModel {
        id: shModel
        ListElement { name: "Inline"; value: ListView.InlineHeader }
        ListElement { name: "Overlay"; value: ListView.OverlayHeader }
        ListElement { name: "PullBack"; value: ListView.PullBackHeader }
    }

    ListModel {
        id: sfModel
        ListElement { name: "Inline"; value: ListView.InlineFooter }
        ListElement { name: "Overlay"; value: ListView.OverlayFooter }
        ListElement { name: "PullBack"; value: ListView.PullBackFooter }
    }

    statusBar: RowLayout {
        anchors.margins: window.hspacing
        anchors.left: parent.left
        anchors.right: parent.right
        Text {
            anchors.left: parent.left
            text: listview.currentSection ? "#" + listview.currentSection : ""
            visible: scsItem.checked || snsItem.checked || isItem.checked
        }
        Text {
            anchors.right: parent.right
            property string pos: listview.isVertical ? listview.contentY.toFixed(2) : listview.contentX.toFixed(2)
            property string size: listview.isVertical ? listview.contentHeight.toFixed(2) : listview.contentWidth.toFixed(2)
            function padded(str) {
                return String("        " + str).slice(-8)
            }
            text: padded(pos) + " /" + padded(size)
        }
    }

    ListView {
        id: listview

        property bool isVertical: orientation == ListView.Vertical

        anchors.fill: parent
        anchors.leftMargin: window.hspacing
        anchors.rightMargin: window.hspacing
        anchors.topMargin: window.vspacing
        anchors.bottomMargin: window.vspacing

        clip: clipItem.checked

        orientation: oriItem.currentIndex === 0 ? ListView.Horizontal : ListView.Vertical
        layoutDirection: ldItem.currentIndex === 0 ? ListView.LeftToRight : ListView.RightToLeft
        verticalLayoutDirection: vldItem.currentIndex === 0 ? ListView.TopToBottom : ListView.BottomToTop

        model: ListModel {
            Component.onCompleted: {
                for (var i = 0; i < 100; ++i)
                    append({section: Math.floor(i / 10)})
            }
        }

        delegate: Rectangle {
            clip: true
            property bool filterOut: filterItem.checked && index % 5
            visible: !filterOut
            width: filterOut ? 0 : parent && listview.isVertical ? parent.width : label.implicitHeight
            height: filterOut ? 0 : !parent || listview.isVertical ? label.implicitHeight : parent.height
            color: index % 2 ? "#ffffff" : "#f3f3f3"
            Text {
                id: label
                anchors.centerIn: parent
                rotation: listview.isVertical ? 0 : -90
                text: index
            }
        }

        section.property: "section"
        section.delegate: Rectangle {
            width: parent && listview.orientation == ListView.Vertical ? parent.width : sectionLabel.implicitHeight
            height: !parent || listview.orientation == ListView.Vertical ? sectionLabel.implicitHeight : parent.height
            color: "darkgray"
            opacity: opacityItem.checked ? 1.0 : 0.8
            Text {
                id: sectionLabel
                anchors.centerIn: parent
                rotation: listview.isVertical ? 0 : -90
                text: "#" + section
            }
        }
        section.labelPositioning: (isItem.checked ? ViewSection.InlineLabels : 0) |
                                  (scsItem.checked ? ViewSection.CurrentLabelAtStart : 0) |
                                  (snsItem.checked ? ViewSection.NextLabelAtEnd : 0)

        headerPositioning: shModel.get(shItem.currentIndex).value
        header: hItem.checked ? headerComponent : null

        footerPositioning: sfModel.get(sfItem.currentIndex).value
        footer: fItem.checked ? footerComponent : null

        Rectangle {
            border.width: 1
            anchors.fill: parent
            color: "transparent"
            border.color: "darkgray"
        }

        Component {
            id: headerComponent
            Rectangle {
                z: 3
                width: parent && listview.orientation == ListView.Vertical ? parent.width : headerLabel.implicitHeight * 2
                height: !parent || listview.orientation == ListView.Vertical ? headerLabel.implicitHeight * 2 : parent.height
                color: "steelblue"
                opacity: opacityItem.checked ? 1.0 : 0.8
                Text {
                    id: headerLabel
                    text: "Header"
                    font.pointSize: 12
                    anchors.centerIn: parent
                    rotation: listview.isVertical ? 0 : -90
                }
            }
        }

        Component {
            id: footerComponent
            Rectangle {
                z: 3
                width: parent && listview.orientation == ListView.Vertical ? parent.width : footerLabel.implicitHeight * 2
                height: !parent || listview.orientation == ListView.Vertical ? footerLabel.implicitHeight * 2 : parent.height
                color: "steelblue"
                opacity: opacityItem.checked ? 1.0 : 0.8
                Text {
                    id: footerLabel
                    text: "Footer"
                    font.pointSize: 10
                    anchors.centerIn: parent
                    rotation: listview.isVertical ? 0 : -90
                }
            }
        }
    }
}
