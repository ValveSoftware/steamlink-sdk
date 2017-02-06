import QtQuick 2.0

Flickable {
    id: flick

    width: 200
    height: 480

    contentHeight: 100 * 100

    Grid {
        columns: 1
        Repeater {
            model: 100
            Rectangle {
                width: flick.width
                height: 100
                color: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
            }
        }
    }
}
