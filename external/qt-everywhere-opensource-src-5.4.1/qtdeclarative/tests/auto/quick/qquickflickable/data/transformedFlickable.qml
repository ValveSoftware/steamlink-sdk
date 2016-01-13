import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        x: 100
        y: 100
        width: 200
        height: 200
        rotation: 45

        Rectangle {
            scale: 0.5
            color: "#888888"
            anchors.fill: parent

            Flickable {
                id: flickable
                contentHeight: 300
                anchors.fill: parent
                objectName: "flickable"

                property alias itemPressed: mouseArea.pressed

                Rectangle {
                    x: 50
                    y: 50
                    width: 100
                    height: 100
                    scale: 0.4
                    color: "red"

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                    }
                }
            }
        }
    }
}
