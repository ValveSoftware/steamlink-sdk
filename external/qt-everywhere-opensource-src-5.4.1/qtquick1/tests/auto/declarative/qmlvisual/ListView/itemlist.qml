// This example demonstrates placing items in a view using
// a VisualItemModel

import QtQuick 1.0

Rectangle {
    color: "lightgray"
    width: 240
    height: 320

    VisualItemModel {
        id: itemModel
        objectName: "itemModel"
        Rectangle {
            objectName: "item1"
            height: view.height; width: view.width; color: "#FFFEF0"
        }
        Rectangle {
            objectName: "item2"
            height: view.height; width: view.width; color: "#F0FFF7"
        }
        Rectangle {
            objectName: "item3"
            height: view.height; width: view.width; color: "#F4F0FF"
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
