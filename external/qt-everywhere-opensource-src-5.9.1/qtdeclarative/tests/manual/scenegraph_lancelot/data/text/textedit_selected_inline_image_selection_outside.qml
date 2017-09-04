import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.pixelSize: 16
        width: parent.width
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        textFormat: Text.RichText
        text: "This is selected from here and <img width=16 height=16 src=\"data/logo.png\" /> to here but not further"

        Component.onCompleted: {
            textEdit.select(2, 3)
        }
    }
}
