import QtQuick 2.0

Item {
    width: 200; height: 200

    function switchComponent() {
        load.sourceComponent = comp2
    }

    Component {
        id: comp
        Rectangle {
            width: 50; height: 60; color: "red"
        }
    }

    Component {
        id: comp2
        Rectangle {
            width: 80; height: 90; color: "green"
        }
    }

    Loader {
        id: load
        objectName: "loader"
        sourceComponent: comp
        height: item ? item.height : 0
    }
}
