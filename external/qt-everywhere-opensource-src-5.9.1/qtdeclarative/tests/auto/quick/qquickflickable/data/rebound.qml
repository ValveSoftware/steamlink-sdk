import QtQuick 2.0

Flickable {
    id: flick

    property int transitionDuration: 100
    property bool transitionEnabled: true
    property int transitionsStarted

    width: 200
    height: 200

    contentWidth: width * 1.25
    contentHeight: width * 1.25

    rebound: Transition {
        objectName: "rebound"
        enabled: flick.transitionEnabled
        SequentialAnimation {
            PropertyAction { target: flick; property: "transitionsStarted"; value: flick.transitionsStarted + 1 }
            NumberAnimation {
                properties: "x,y"
                easing.type: Easing.OutElastic
                duration: flick.transitionDuration
            }
        }
    }

    Grid {
        columns: 2
        Repeater {
            model: 4
            Rectangle {
                width: flick.contentWidth/2
                height: flick.contentHeight/2
                color: Qt.rgba(Math.random(), Math.random(), Math.random(), 1)
            }
        }
    }
}

