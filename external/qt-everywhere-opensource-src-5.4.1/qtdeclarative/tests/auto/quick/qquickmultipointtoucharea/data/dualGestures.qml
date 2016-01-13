/*  From the docs about minimumTouchPoints/maximumTouchPoints:
    "...allow you to, for example, have nested MultiPointTouchAreas,
    one handling two finger touches, and another handling three finger touches."
    But in this test they are side-by-side: the left one handles any number
    of touches up to 2, and the right one requires 3.
*/
import QtQuick 2.0

Row {
    width: 640
    height: 480

    Rectangle {
        color: "black"
        border.color: "white"
        height: parent.height
        width: parent.width / 2
        MultiPointTouchArea {
            objectName: "dualTouchArea"
            anchors.fill: parent
            maximumTouchPoints: 2
            touchPoints: [
                TouchPoint { id: touch1 },
                TouchPoint { id: touch2 }
            ]
            Rectangle {
                objectName: "touch1rect"
                color: "red"
                width: 30
                height: width
                radius: width / 2
                x: touch1.x
                y: touch1.y
                border.color: touch1.pressed ? "white" : "transparent"
            }
            Rectangle {
                objectName: "touch2rect"
                color: "yellow"
                width: 30
                height: width
                radius: width / 2
                x: touch2.x
                y: touch2.y
                border.color: touch2.pressed ? "white" : "transparent"
            }
        }
    }


    Rectangle {
        color: "black"
        border.color: "white"
        height: parent.height
        width: parent.width / 2
        MultiPointTouchArea {
            objectName: "tripleTouchArea"
            anchors.fill: parent
            minimumTouchPoints: 3
            maximumTouchPoints: 3
            touchPoints: [
                TouchPoint { id: touch3 },
                TouchPoint { id: touch4 },
                TouchPoint { id: touch5 }
            ]
            Rectangle {
                objectName: "touch3rect"
                color: "green"
                width: 30
                height: width
                x: touch3.x
                y: touch3.y
                border.color: touch3.pressed ? "white" : "transparent"
            }
            Rectangle {
                objectName: "touch4rect"
                color: "blue"
                width: 30
                height: width
                x: touch4.x
                y: touch4.y
                border.color: touch4.pressed ? "white" : "transparent"
            }
            Rectangle {
                objectName: "touch5rect"
                color: "violet"
                width: 30
                height: width
                x: touch5.x
                y: touch5.y
                border.color: touch5.pressed ? "white" : "transparent"
            }
        }
    }
}
