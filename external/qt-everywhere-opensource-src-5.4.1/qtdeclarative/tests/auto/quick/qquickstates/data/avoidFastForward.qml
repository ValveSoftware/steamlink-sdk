import QtQuick 2.0

Rectangle {
    id: rect
    width: 200
    height: 200

    property int updateCount: 0
    onColorChanged: updateCount++

    property color aColor: "green"

    states: State {
        name: "a"
        PropertyChanges { target: rect; color: aColor }
    }
}
