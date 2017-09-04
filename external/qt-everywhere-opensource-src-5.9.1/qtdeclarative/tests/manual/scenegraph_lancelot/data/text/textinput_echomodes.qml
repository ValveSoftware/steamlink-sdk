import QtQuick 2.0

Item {
    width: 320
    height: 480

    Column {
        anchors.centerIn: parent

        TextInput {
            font.pixelSize: 16
            font.family: "Arial"
            echoMode: TextInput.Normal
            text: "Lorem ipsum"
        }
        TextInput {
            font.pixelSize: 16
            font.family: "Arial"
            echoMode: TextInput.NoEcho
            text: "Lorem ipsum"
        }
        TextInput {
            font.pixelSize: 16
            font.family: "Arial"
            echoMode: TextInput.Password
            text: "Lorem ipsum"
        }
        TextInput {
            font.pixelSize: 16
            font.family: "Arial"
            echoMode: TextInput.PasswordEchoOnEdit
            text: "Lorem ipsum"
        }
        TextInput {
            font.pixelSize: 16
            font.family: "Arial"
            echoMode: TextInput.Password
            passwordCharacter: "o"
            text: "Lorem ipsum"
        }
    }
}
