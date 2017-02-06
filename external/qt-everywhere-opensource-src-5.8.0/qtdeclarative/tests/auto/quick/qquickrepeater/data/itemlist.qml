// This example demonstrates placing items in a view using
// a VisualItemModel

import QtQuick 2.0

Rectangle {
    id: root
    color: "lightgray"
    width: 240
    height: 320
    property variant itemModel: itemModel1

    function checkProperties() {
        testObject.error = false;
        if (testObject.useModel && view.model != root.itemModel) {
            console.log("model property incorrect");
            testObject.error = true;
        }
    }

    function switchModel() {
        root.itemModel = itemModel2
    }

    VisualItemModel {
        id: itemModel1
        objectName: "itemModel1"
        Rectangle {
            objectName: "item1"
            height: 50; width: 100; color: "#FFFEF0"
            Text { objectName: "text1"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item2"
            height: 50; width: 100; color: "#F0FFF7"
            Text { objectName: "text2"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item3"
            height: 50; width: 100; color: "#F4F0FF"
            Text { objectName: "text3"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
    }

    VisualItemModel {
        id: itemModel2
        objectName: "itemModel2"
        Rectangle {
            objectName: "item4"
            height: 50; width: 100; color: "#FFFEF0"
            Text { objectName: "text4"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
        Rectangle {
            objectName: "item5"
            height: 50; width: 100; color: "#F0FFF7"
            Text { objectName: "text5"; text: "index: " + parent.VisualItemModel.index; font.bold: true; anchors.centerIn: parent }
        }
    }

    Column {
        objectName: "container"
        Repeater {
            id: view
            objectName: "repeater"
            model: testObject.useModel ? root.itemModel : 0
        }
    }
}
