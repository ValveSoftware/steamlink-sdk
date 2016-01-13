import QtQuick 2.0

Rectangle {
    id: root
    width: 100; height: 100
    property bool clicked: false

    Flickable {
        objectName: "flickable"
        width: 100; height: 100
        contentWidth: column.width; contentHeight: column.height
        enabled: false

        Column {
            id: column
            Repeater {
                model: 4
                Rectangle {
                    width: 200; height: 300; color: "blue"
                    MouseArea { anchors.fill: parent; onClicked: {  } }
                }
            }
        }
    }

    MouseArea {
        width: 100; height: 30
        onClicked: root.clicked = true
    }
}
