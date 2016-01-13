// This example demonstrates how item positioning
// changes in right-to-left layout direction

import QtQuick 2.0

Rectangle {
    color: "lightgray"
    width: 340
    height: 370

    VisualItemModel {
        id: itemModel
        objectName: "itemModel"
        Rectangle {
            objectName: "item1"
            height: 110; width: 120; color: "#FFFEF0"
            Text { objectName: "text1"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item2"
            height: 130; width: 150; color: "#F0FFF7"
            Text { objectName: "text2"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item3"
            height: 170; width: 190; color: "#F4F0FF"
            Text { objectName: "text3"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
    }

    GridView {
        id: view
        objectName: "view"
        cellWidth: 190
        cellHeight: 170
        anchors.fill: parent
        anchors.bottomMargin: 30
        model: itemModel
        highlightRangeMode: "StrictlyEnforceRange"
        flow: GridView.TopToBottom
        flickDeceleration: 2000
    }
}
