import QtQuick 2.0

Item {
    width: 320
    height: 480
    TextInput {
        anchors.centerIn: parent
        id: textInput
        font.family: "Arial"
        font.pixelSize: 14
        text: "∯AA≨"

        Component.onCompleted: {
            textInput.select(2, 4)
        }
    }
}
