import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    Rectangle {
        id: myItem
        objectName:  "myItem"
        width: 100
        height: 100
        color: "green"
        x: 100 - myItem.y

        Binding on x {
            when: myItem.y > 50
            value: myItem.y
        }

        /*NumberAnimation on y {
            loops: Animation.Infinite
            to: 100
            duration: 1000
        }*/
    }
}
