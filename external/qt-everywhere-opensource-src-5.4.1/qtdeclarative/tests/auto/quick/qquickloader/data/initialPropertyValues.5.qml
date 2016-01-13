import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property int behaviorCount: 0

    Loader {
        id: loader
        objectName: "loader"
        onLoaded: {
            root.initialValue = loader.item.canary;         // should be zero, but no error
            root.behaviorCount = loader.item.behaviorCount; // should be zero
        }
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml");
    }
}
