import QtQuick 2.0

Rectangle {
    width: 400
    height: 200

    property variant pixelSize: 6

    Text {
        objectName: "text"
        font.pixelSize: parent.pixelSize
        text: "This is<br/>a font<br/> size test."
    }

    Text {
        x: 200
        objectName: "textWithTag"
        font.pixelSize: parent.pixelSize
        text: "This is <h4>a font</h4> size test."
    }
}
