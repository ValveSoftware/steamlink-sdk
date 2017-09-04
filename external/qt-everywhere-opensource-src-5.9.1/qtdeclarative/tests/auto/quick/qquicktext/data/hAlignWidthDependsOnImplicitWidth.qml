import QtQuick 2.6

Item {
    width: 200
    height: 200

    property alias text: label.text
    property alias horizontalAlignment: label.horizontalAlignment
    property alias elide: label.elide
    property int extraWidth: 0

    Rectangle {
        border.color: "red"
        x: 100
        width: label.implicitWidth + extraWidth
        height: label.implicitHeight

        Text {
            id: label
            anchors.fill: parent
            text: 'press me'
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
