import QtQuick 2.0

MultiPointTouchArea {
    width: 240
    height: 320

    property bool grabInnerArea: true

    Rectangle {
        color: "red"
        width: 30
        height: width
        radius: width / 2
        x: point11.x
        y: point11.y
        border.color: point11.pressed ? "white" : "transparent"
    }
    Rectangle {
        objectName: "touch2rect"
        color: "yellow"
        width: 30
        height: width
        radius: width / 2
        x: point12.x
        y: point12.y
        border.color: point12.pressed ? "white" : "transparent"
    }

    Rectangle {
        color: "orange"
        width: 30
        height: width
        radius: width / 2
        x: point21.x
        y: point21.y
        border.color: point21.pressed ? "white" : "transparent"
    }
    Rectangle {
        objectName: "touch2rect"
        color: "green"
        width: 30
        height: width
        radius: width / 2
        x: point22.x
        y: point22.y
        border.color: point22.pressed ? "white" : "transparent"
    }
    Rectangle {
        color: "blue"
        width: 30
        height: width
        radius: width / 2
        x: point23.x
        y: point23.y
        border.color: point23.pressed ? "white" : "transparent"
    }

    minimumTouchPoints: 2
    maximumTouchPoints: 3
    touchPoints: [
        TouchPoint { id: point11; objectName: "point11" },
        TouchPoint { id: point12; objectName: "point12" }
    ]

    MultiPointTouchArea {
        anchors.fill: parent
        minimumTouchPoints: 3
        maximumTouchPoints: 3
        onGestureStarted: if (grabInnerArea) gesture.grab()
        touchPoints: [
            TouchPoint { id: point21; objectName: "point21" },
            TouchPoint { id: point22; objectName: "point22" },
            TouchPoint { id: point23; objectName: "point23" }
        ]
    }
}
