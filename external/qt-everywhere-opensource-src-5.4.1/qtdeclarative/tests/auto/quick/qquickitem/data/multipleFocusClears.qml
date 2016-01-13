import QtQuick 2.0

Rectangle {
    width: 200
    height: 200

    FocusScope {
        id: focusScope
        anchors.fill: parent

        TextInput {
            anchors.centerIn: parent
            text: "Some text"
            onActiveFocusChanged: if (!activeFocus) focusScope.focus = false
            Component.onCompleted: forceActiveFocus()
        }
    }
}
