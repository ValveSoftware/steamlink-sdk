import QtQuick 2.0

Item {
    width: 300
    height: 200

    Rectangle {
        anchors.fill: myText
        border.width: 1
    }

    Text {
        id: myText
        objectName: "myText"
        width: 250
        height: 41
        minimumPointSize: 8
        minimumPixelSize: 8
        font.pixelSize: 30
        font.family: "Helvetica"
    }
}
