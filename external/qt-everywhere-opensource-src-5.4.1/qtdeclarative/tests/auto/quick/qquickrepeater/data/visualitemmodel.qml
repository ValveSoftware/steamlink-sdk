import QtQuick 2.0

Rectangle {
    width: 360
    height: 360

    VisualItemModel {
        id: visItemModel
        Rectangle {
            width: 20
            height: 20
            color: "red"
        }
    }

    Column {
        anchors.fill: parent
        Repeater {
            model: visItemModel
        }
    }
}

