import QtQuick 2.0

Rectangle {
    width: 400
    height: 400
    color: "gray"

    Flickable {
        id: flick
        objectName: "flick"
        property bool ended: false
        property int movementsAfterEnd: 0
        anchors.fill: parent
        contentWidth: 800
        contentHeight: 800
        onContentXChanged: if (ended) ++movementsAfterEnd
        onContentYChanged: if (ended) ++movementsAfterEnd
        onMovementEnded: ended = true

        Rectangle {
            width: flick.contentWidth
            height: flick.contentHeight
            color: "red"
            Rectangle {
                width: 50; height: 50; color: "blue"
                anchors.centerIn: parent
            }
        }
    }
}
