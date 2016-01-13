import QtQuick 2.1

Item {
    id: main
    objectName: "main"
    width: 300
    height: 300
    TextInput {
        id: textinput1
        objectName: "textinput1"
        width: 300
        height: 75
        activeFocusOnTab: true
        text: "Text Input 1"
        anchors.top: parent.top
        anchors.left: parent.left
    }
    TextInput {
        id: textinput2
        objectName: "textinput2"
        width: 300
        height: 75
        activeFocusOnTab: true
        text: "Text Input 2"
        readOnly: true
        anchors.top: textinput1.bottom
        anchors.left: parent.left
    }
    TextEdit {
        id: textedit1
        objectName: "textedit1"
        width: 300
        height: 75
        activeFocusOnTab: true
        text: "Text Edit 1"
        anchors.top: textinput2.bottom
        anchors.left: parent.left
    }
    TextEdit {
        id: textedit2
        objectName: "textedit2"
        width: 300
        height: 75
        activeFocusOnTab: true
        text: "Text Edit 2"
        readOnly: true
        anchors.top: textedit1.bottom
        anchors.left: parent.left
    }
}
