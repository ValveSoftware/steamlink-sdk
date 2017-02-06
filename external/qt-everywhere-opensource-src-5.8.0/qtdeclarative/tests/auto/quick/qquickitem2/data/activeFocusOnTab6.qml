import QtQuick 2.1

Item {
    id: main
    objectName: "main"
    width: 800
    height: 600
    focus: true
    Component.onCompleted: button12.focus = true
    Item {
        id: sub1
        objectName: "sub1"
        width: 230
        height: 600
        anchors.top: parent.top
        anchors.left: parent.left
        Item {
            id: button11
            objectName: "button11"
            width: 100
            height: 50
            activeFocusOnTab: true
            Accessible.role: Accessible.Table
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }

            anchors.top: parent.top
            anchors.topMargin: 100
        }
        Item {
            id: button12
            objectName: "button12"
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
            width: 100
            height: 50

            anchors.top: button11.bottom
            anchors.bottomMargin: 100
        }
        Item {
            id: button13
            objectName: "button13"
            enabled: false
            activeFocusOnTab: true
            Accessible.role: Accessible.Table
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
            width: 100
            height: 50

            anchors.top: button12.bottom
            anchors.bottomMargin: 100
        }
        Item {
            id: button14
            objectName: "button14"
            visible: false
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
            width: 100
            height: 50

            anchors.top: button12.bottom
            anchors.bottomMargin: 100
        }
    }
    Item {
        id: sub2
        objectName: "sub2"
        activeFocusOnTab: true
        Accessible.role: Accessible.Row
        width: 230
        height: 600
        anchors.top: parent.top
        anchors.left: sub1.right
        Item {
            id: button21
            objectName: "button21"
            width: 100
            height: 50
            activeFocusOnTab: true
            Accessible.role: Accessible.PushButton
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }

            anchors.top: parent.top
            anchors.topMargin: 100
        }
        Item {
            id: button22
            objectName: "button22"
            width: 100
            height: 50
            activeFocusOnTab: true
            Accessible.role: Accessible.RadioButton
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }

            anchors.top: button21.bottom
            anchors.bottomMargin: 100
        }
    }
    Item {
        id: sub3
        objectName: "sub3"
        width: 230
        height: 600
        anchors.top: parent.top
        anchors.left: sub2.right
        TextEdit {
            id: edit
            objectName: "edit"
            width: 230
            height: 400
            readOnly: false
            activeFocusOnTab: true
            Accessible.role: Accessible.EditableText
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.RichText

            text: "aaa\n"
                +"bbb\n"
                +"ccc\n"
                +"ddd\n"
        }
    }
}
