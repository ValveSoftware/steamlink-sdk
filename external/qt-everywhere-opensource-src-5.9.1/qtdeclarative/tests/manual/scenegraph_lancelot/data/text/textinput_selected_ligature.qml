import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextInput {
        anchors.centerIn: parent
        id: textInput
        font.pixelSize: 12
        font.family: "Arial"
        text: "Are griffins birds or mammals?"

        Component.onCompleted: {
            textInput.select(3, 8)
        }
    }
}
