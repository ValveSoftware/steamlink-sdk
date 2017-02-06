import QtQuick 2.0

Item {
    width: 320
    height: 480
    TextInput {
        anchors.centerIn: parent
        id: textInput
        font.pixelSize: 44
        font.italic: true
        text: "777"

        Component.onCompleted: {
            textInput.select(1, 2)
        }
    }
}
