import QtQuick 2.0
import Test 1.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        x: 100
        y: 100
        width: 200
        height: 200
        rotation: 45

        Rectangle {
            scale: 0.5
            color: "black"
            anchors.fill: parent
            radius: hollowItem.circle ? 100 : 0

            Rectangle {
                color: "white"
                anchors.centerIn: parent
                width: hollowItem.holeRadius * 2
                height: hollowItem.holeRadius * 2
                radius: hollowItem.circle ? 100 : 0
            }

            HollowTestItem {
                id: hollowItem
                anchors.fill: parent
                objectName: "hollowItem"
                holeRadius: 50
                circle: circleShapeTest
            }
        }
    }
}
