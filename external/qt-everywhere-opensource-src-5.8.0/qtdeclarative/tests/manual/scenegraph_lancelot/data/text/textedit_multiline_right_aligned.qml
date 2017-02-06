import QtQuick 2.0

Item {
    width: 320
    height: 480
    clip: true

    TextEdit {
        id: textEdit
        anchors.centerIn: parent
        font.pixelSize: 16
        width: 200
        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
        horizontalAlignment: TextEdit.AlignRight
        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. In id diam vitae enim fringilla vestibulum. Pellentesque non leo justo, quis vestibulum augue"
    }

}
