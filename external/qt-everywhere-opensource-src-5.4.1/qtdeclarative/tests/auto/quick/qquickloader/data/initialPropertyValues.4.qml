import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property int behaviorCount: 0

    Loader {
        id: loader
        objectName: "loader"
        active: false
        onLoaded: {
            root.initialValue = loader.item.canary;         // should be four
            root.behaviorCount = loader.item.behaviorCount; // should be zero
        }
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", {"canary": 4});
        loader.active = true
    }
}
