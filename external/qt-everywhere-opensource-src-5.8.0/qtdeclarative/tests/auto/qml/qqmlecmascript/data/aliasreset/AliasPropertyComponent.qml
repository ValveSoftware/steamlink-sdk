import QtQuick 2.0

Item {
    id: apc
    property alias sourceComponent: loader.sourceComponent

    Component {
        id: redSquare
        Rectangle { color: "red"; width: 10; height: 10 }
    }

    Loader {
        id: loader
        objectName: "loader"
        sourceComponent: redSquare
    }
}
