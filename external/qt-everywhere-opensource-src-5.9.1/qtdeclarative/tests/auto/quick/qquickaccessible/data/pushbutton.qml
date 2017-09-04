import QtQuick 2.0

Rectangle {
    Accessible.role : Accessible.Button
    property string text : "test"

    Text {
        anchors.fill : parent
        text : parent.text
    }

    MouseArea {
        anchors.fill : parent
    }
}
