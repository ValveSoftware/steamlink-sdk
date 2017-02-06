import QtQuick 2.0

Item {
    width: 200
    height: 200

    property int behaviorCount: 0

    Rectangle {
        id: myRect
        objectName: "myRect"
        width: 100
        height: 100
        Behavior on x {
            ScriptAction { script: ++behaviorCount }
        }
    }
}
