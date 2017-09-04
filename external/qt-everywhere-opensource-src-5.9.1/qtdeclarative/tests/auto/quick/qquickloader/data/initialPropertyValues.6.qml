import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property int behaviorCount: 0

    Loader {
        id: loader
        objectName: "loader"
        onLoaded: {
            root.initialValue = loader.item.canary;         // should be six
            root.behaviorCount = loader.item.behaviorCount; // should be zero
        }
    }

    Item {
        id: child
        property int bindable: 6
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", {"canary": child.bindable});
    }
}
