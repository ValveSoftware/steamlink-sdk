// This example demonstrates how item positioning
// changes in right-to-left layout direction

import QtQuick 1.1

Rectangle {
    color: "lightgray"
    width: 640
    height: 320

    VisualItemModel {
        id: itemModel
        objectName: "itemModel"
        Rectangle {
            objectName: "item1"
            height: view.height; width: 100; color: "#FFFEF0"
            Text { objectName: "text1"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item2"
            height: view.height; width: 200; color: "#F0FFF7"
            Text { objectName: "text2"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item3"
            height: view.height; width: 240; color: "#F4F0FF"
            Text { objectName: "text3"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
    }

    ListView {
        id: view
        objectName: "view"
        anchors.fill: parent
        anchors.bottomMargin: 30
        model: itemModel
        highlightRangeMode: "StrictlyEnforceRange"
        orientation: ListView.Horizontal
        flickDeceleration: 2000
        layoutDirection: Qt.RightToLeft
    }
}
