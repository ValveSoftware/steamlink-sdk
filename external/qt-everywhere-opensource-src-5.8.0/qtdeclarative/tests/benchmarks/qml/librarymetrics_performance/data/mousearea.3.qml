import QtQuick 2.0

MouseArea {
    width: 320
    height: 480
    property int clickCount: 0
    onClicked: {
        clickCount++;
    }
}
