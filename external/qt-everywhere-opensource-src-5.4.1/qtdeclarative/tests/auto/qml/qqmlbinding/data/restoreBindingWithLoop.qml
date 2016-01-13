import QtQuick 2.0

Rectangle {
    width: 400
    height: 400

    property bool activateBinding: false

    Rectangle {
        id: myItem
        objectName:  "myItem"
        width: 100
        height: 100
        color: "green"
        x: myItem.y + 100
        onXChanged: { if (x == 188) y = 90; }   //create binding loop

        Binding on x {
            when: activateBinding
            value: myItem.y
        }
    }
}
