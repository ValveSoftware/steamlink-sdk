import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Text {
        id: text
        anchors.centerIn: parent
        font.pointSize: 24
        Behavior on font.pointSize { NumberAnimation {} }
    }

    function updateFontProperties() {
        text.font.italic = true
        text.font.pointSize = 48
        text.font.weight = Font.Bold
    }
}
