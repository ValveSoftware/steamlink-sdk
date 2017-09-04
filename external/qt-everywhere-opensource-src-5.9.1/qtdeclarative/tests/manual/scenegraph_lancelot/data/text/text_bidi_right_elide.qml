import QtQuick 2.0

Item {
    width: 320
    height: 480

    Text {
        anchors.centerIn: parent
        font.family: "Arial"
        font.pixelSize: 20
        text: "هو أمّا حكومة القاذفات مكن"
        elide: Text.ElideRight
        width: 100
        wrapMode: Text.NoWrap
    }
}
