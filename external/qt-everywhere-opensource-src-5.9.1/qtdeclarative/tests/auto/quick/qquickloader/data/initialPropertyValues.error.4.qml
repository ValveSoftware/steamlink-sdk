import QtQuick 2.0

Item {
    id: root
    property int canary: loader.item.canary

    Loader {
        id: loader
        objectName: "loader"
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", 3); // invalid initial properties object
    }
}
