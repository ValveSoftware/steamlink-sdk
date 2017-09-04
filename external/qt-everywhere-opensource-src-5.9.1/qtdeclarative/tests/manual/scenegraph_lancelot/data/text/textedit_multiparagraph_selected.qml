import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 20
        textFormat: Text.RichText
        text: "<p>First line</p><p>Second line</p><p>Third line</p><p>Fourth line</p>"

        Component.onCompleted: {
            textEdit.select(18, 26)
        }
    }

}
