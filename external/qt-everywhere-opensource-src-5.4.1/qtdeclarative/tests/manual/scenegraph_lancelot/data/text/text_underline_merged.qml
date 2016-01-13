import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 16
        textFormat: Text.RichText
        width: parent.width
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        text: "<u>Lorem</u> ipsum <u><font size=20>dolor sit amet</font>, consect</u>etur."
    }
}
