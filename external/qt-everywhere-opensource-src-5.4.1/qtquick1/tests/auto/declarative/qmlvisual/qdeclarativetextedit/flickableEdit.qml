import QtQuick 1.0

Flickable {
    width: 200
    height: 50
    contentWidth: 400

    Column {
        anchors.fill: parent

        TextEdit {
            selectByMouse: true
            text: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        }
        TextEdit {
            selectByMouse: false
            text: "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        }
    }
}
