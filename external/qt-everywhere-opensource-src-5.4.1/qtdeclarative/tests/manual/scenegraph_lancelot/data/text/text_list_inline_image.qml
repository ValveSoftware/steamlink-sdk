import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "Here's a list: <ul><li>List item 1</li><li>List <img height=16 width=16 src=\"data/logo.png\" />item 2</li><li>List item 3</li></ul> And some more text"
    }
}
