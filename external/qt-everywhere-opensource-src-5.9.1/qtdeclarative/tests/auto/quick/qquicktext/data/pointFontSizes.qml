import QtQuick 2.0

Rectangle {
    width: 400
    height: 200

    property variant pointSize: 6

    Text {
        objectName: "text"
        font.pointSize: parent.pointSize
        text: "This is<br/>a font<br/> size test."
    }

    Text {
        x: 200
        objectName: "textWithTag"
        font.pointSize: parent.pointSize
        text: "This is <h4>a font</h4> size test."
    }
}
