import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        width: parent.width
        anchors.centerIn: parent
        font.pixelSize: 20
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        text: "<i>Lorem ipsum dolor sit amet</i>, consectetur <b>adipiscing</b> elit. Maecenas nibh."
        font.family: "Arial"
    }
}
