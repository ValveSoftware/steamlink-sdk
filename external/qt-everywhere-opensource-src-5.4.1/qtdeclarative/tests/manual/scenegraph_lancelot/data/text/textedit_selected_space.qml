import QtQuick 2.0

Item {
    width: 320
    height: 480

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        text: "Lorem Ipsum (space should be selected)"

        Component.onCompleted: {
            textEdit.select(5, 6)
        }
    }

}
