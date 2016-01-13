import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property int behaviorCount: 0

    Loader {
        id: loader
        objectName: "loader"

        onLoaded: {
            loader.item.canary = 1; // will trigger the behavior, setting behaviorCount -> 1
        }
    }

    Component.onCompleted: {
        loader.source = "InitialPropertyValuesComponent.qml";
        root.initialValue = loader.item.canary;         // should be one, since onLoaded will have triggered by now
        root.behaviorCount = loader.item.behaviorCount; // should be one, since onLoaded will have triggered by now
    }
}
