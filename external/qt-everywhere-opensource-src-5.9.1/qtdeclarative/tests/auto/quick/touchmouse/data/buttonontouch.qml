import QtQuick 2.0
import Qt.test 1.0

Rectangle {
    id: root
    width: 300
    height: 800
    color: "green"

    Rectangle {
        color: "blue"
        height: 400
        width: parent.width


        PinchArea {
            pinch.target: button1
            objectName: "pincharea"
            anchors.fill: parent

            pinch.minimumScale: 0.1
            pinch.maximumScale: 10.0
        }

        Rectangle {

            id: button1
            objectName: "button1"
            y: 100
            height: 100
            width: parent.width
            Text { text: "Button 1" }

            EventItem {
                objectName: "eventItem1"
                height: 100
                width: 300
            }
        }

        Rectangle {
            objectName: "button2"
            y: 300
            height: 100
            width: parent.width
            Text { text: "Button 2" }

            EventItem {
                objectName: "eventItem2"
                height: 100
                width: 300
            }
        }
    }

    Rectangle {
        y: 400
        width: parent.width
        height: parent.height
        color: "red"

        MultiPointTouchArea {
            objectName: "toucharea"
            anchors.fill: parent

            y: 400
            height: 400

            Rectangle {
                objectName: "button3"
                y: 100
                height: 100
                width: parent.width
                Text { text: "Button 3" }

                EventItem {
                    objectName: "eventItem3"
                    height: 100
                    width: 300
                }
            }

            Rectangle {
                objectName: "button4"
                y: 300
                height: 100
                width: parent.width
                Text { text: "Button 4" }

                EventItem {
                    objectName: "eventItem4"
                    height: 100
                    width: 300
                }
            }

        }
    }
}

