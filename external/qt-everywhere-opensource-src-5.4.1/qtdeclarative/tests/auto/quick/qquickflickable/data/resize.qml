import QtQuick 2.0

Rectangle {
    function resizeContent() {
        flick.resizeContent(600, 600, Qt.point(100, 100))
    }
    function returnToBounds() {
        flick.returnToBounds()
    }
    width: 400
    height: 360
    color: "gray"

    Flickable {
        id: flick
        objectName: "flick"
        anchors.fill: parent
        contentWidth: 300
        contentHeight: 300

        rebound: setRebound ? boundsTransition : null
        Transition {
            id: boundsTransition
            objectName: "rebound"
            NumberAnimation {
                properties: "x,y"
                easing.type: Easing.OutElastic
            }
        }

        Rectangle {
            width: flick.contentWidth
            height: flick.contentHeight
            color: "red"
        }
    }
}
