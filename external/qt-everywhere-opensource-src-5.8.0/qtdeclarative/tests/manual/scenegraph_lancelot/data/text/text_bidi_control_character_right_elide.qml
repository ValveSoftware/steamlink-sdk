import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 20
        text: "\u202e This is RTL \u202c"
        elide: Text.ElideRight
        width: 100
        wrapMode: Text.NoWrap
    }
}
