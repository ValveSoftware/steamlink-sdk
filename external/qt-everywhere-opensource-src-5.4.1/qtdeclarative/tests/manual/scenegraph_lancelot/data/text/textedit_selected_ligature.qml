import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        anchors.centerIn: parent
        id: textEdit
        font.pixelSize: 16
        font.family: "Arial"
        text: "Are griffins birds or mammals?"

        Component.onCompleted: {
            textEdit.select(3, 8)
        }
    }
}
