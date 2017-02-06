import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        width: parent.width
        font.pixelSize: 14
        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Maecenas nibh"
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere

        selectionColor: "red"
        selectedTextColor: "blue"

        Component.onCompleted: {
            textEdit.select(6, 17)
        }
    }
}
