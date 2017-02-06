import QtQuick 2.0

TextInput {
    text: "Hello world!"
    inputMethodHints: Qt.ImhNoPredictiveText
    Keys.onLeftPressed: {}
}
