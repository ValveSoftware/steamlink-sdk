import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "Here's a list: <ol><li>List item 1</li><li>List item 2</li><li>List item 3</li></ol> And some more text"
    }
}
