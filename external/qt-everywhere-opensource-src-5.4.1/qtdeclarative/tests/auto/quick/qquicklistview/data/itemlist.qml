// This example demonstrates placing items in a view using
// a VisualItemModel

import QtQuick 2.0

Rectangle {
    color: "lightgray"
    width: 240
    height: 320

    VisualItemModel {
        id: itemModel
        objectName: "itemModel"
        Rectangle {
            objectName: "item1"
            height: ListView.view ? ListView.view.height : 0
            width: view.width; color: "#FFFEF0"
            Text { objectName: "text1"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item2"
            height: ListView.view ? ListView.view.height : 0
            width: view.width; color: "#F0FFF7"
            Text { objectName: "text2"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item3"
            height: ListView.view ? ListView.view.height : 0
            width: view.width; color: "#F4F0FF"
            Text { objectName: "text3"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
    }

    ListView {
        id: view
        objectName: "view"
        anchors.fill: parent
        anchors.bottomMargin: 30
        model: itemModel
        preferredHighlightBegin: 0
        preferredHighlightEnd: 0
        highlightRangeMode: "StrictlyEnforceRange"
        orientation: ListView.Horizontal
        flickDeceleration: 2000
    }
}
