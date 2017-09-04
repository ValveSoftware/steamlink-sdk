import QtQuick 2.0
import QtQuick.Window 2.0 as Window

Window.Window
{
    id: win
    visible: true
    width: 250
    height: 250

    SequentialAnimation {
        PauseAnimation { duration: 500 }
        PropertyAction { target: win; property: "visible"; value: true }
        loops: Animation.Infinite
        running: true
    }
}
