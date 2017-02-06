import QtQuick 2.0

Column {
    width: 50; height: 200
    Rectangle {
        id: item1
        objectName: "item1"
        focus: true
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.down: item2
    }
    Rectangle {
        id: item2
        objectName: "item2"
        enabled: false
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.down: item3
    }
    Rectangle {
        id: item3
        objectName: "item3"
        enabled: false
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.down: item4
    }
    Rectangle {
        id: item4
        objectName: "item4"
        enabled: false
        width: 50; height: 50
        color: focus ? "red" : "lightgray"
        KeyNavigation.down: item3
    }
}
