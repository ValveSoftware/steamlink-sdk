import QtQuick 1.0

Flickable {
    width: 200
    height: 50
    contentWidth: 400
    contentHeight: 100

    Column {
        anchors.fill: parent

        TextInput {
            selectByMouse: true
            text: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        }
        TextInput {
            selectByMouse: false
            text: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        }
    }
}
