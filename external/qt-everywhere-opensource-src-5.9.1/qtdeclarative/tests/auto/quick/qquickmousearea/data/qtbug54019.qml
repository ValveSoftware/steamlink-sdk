import QtQuick 2.7

Item {
    width: 200
    height: 200
    MouseArea {
        id: ma
        property string str: "foo!"
        width: 150; height: 150
        hoverEnabled: true

        Rectangle {
            anchors.fill: parent
            color: ma.containsMouse ? "lightsteelblue" : "gray"
        }
        Text {
            text: ma.str
            textFormat: Text.PlainText // consequently Text does not care about hover events
        }
    }
}
