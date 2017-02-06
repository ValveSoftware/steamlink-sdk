import QtQuick 2.0

Item {
    width: 200
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 64
        width: 200
        textFormat: TextEdit.RichText
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        text: "ABC ABC<br>ABC"

        Component.onCompleted: {
            textEdit.selectAll()
        }
    }

}
