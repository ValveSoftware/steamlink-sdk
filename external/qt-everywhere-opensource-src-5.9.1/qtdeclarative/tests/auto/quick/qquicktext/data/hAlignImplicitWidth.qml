import QtQuick 2.0

Rectangle {
    width: 220
    height: 100

    Text {
        objectName: "textItem"
        text: "AA\nBBBBBBB\nCCCCCCCCCCCCCCCC"
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignLeft
    }
}
