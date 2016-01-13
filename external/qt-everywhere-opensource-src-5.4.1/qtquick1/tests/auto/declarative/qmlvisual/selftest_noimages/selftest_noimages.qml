import QtQuick 1.0
Text {
    property string error: "not pressed"
    text: (new Date()).valueOf()
    MouseArea {
        anchors.fill: parent
        onPressed: error=""
    }
}
