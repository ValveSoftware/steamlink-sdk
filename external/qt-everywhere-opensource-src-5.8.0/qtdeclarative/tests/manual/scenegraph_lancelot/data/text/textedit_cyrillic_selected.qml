import QtQuick 2.0

Item {
    width: 320
    height: 480
    TextEdit {
        id: textEdit
        text: "и в у"
        anchors.centerIn: parent
        Component.onCompleted: textEdit.select(2, 3)
        font.pixelSize: 14
    }
}
