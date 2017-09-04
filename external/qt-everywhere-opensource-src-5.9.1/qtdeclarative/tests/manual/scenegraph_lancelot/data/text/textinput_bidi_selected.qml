import QtQuick 2.0

Item {
    width: 320
    height: 480
    TextInput {
        anchors.centerIn: parent
        id: textInput
        font.family: "Arial"
        font.pixelSize: 14
        text: "Lorem ipsum لمّ استبدال dolor sit."

        Component.onCompleted: {
            textInput.select(10, 18)
        }
    }
}
