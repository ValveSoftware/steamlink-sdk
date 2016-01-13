import QtQuick 2.0

Item {
    Component {
        id: item1
        Item {
            objectName: "item1"
        }
    }
    Component {
        id: item2
        Item {
            objectName: "item2"
        }
    }
    Loader {
        id: loader
        sourceComponent: item1
    }
    Connections {
        objectName: "connections"
        target: loader.item
        onWidthChanged: loader.sourceComponent = item2
    }
}
