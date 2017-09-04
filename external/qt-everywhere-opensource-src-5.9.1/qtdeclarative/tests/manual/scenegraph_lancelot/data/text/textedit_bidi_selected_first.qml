import QtQuick 2.0

Item {
    width: 320
    height: 480
    TextEdit {
        anchors.centerIn: parent
        id: textEdit
        font.family: "Arial"
        font.pixelSize: 14
        text: "Lorem ipsum لمّ استبدال dolor sit."

        Component.onCompleted: {
            textEdit.select(0, 1)
        }
    }
}
