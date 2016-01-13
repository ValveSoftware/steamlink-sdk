import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        text: "Here's a list: <ul><li>List item 1</li><li>List item 2</li><li>List item 3</li></ul> And some more text"

        Component.onCompleted: {
            textEdit.select(18, 30)
        }
    }
}
