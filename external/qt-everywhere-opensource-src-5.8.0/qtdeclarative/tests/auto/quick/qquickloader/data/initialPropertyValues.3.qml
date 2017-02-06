import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property int behaviorCount: 0

    Loader {
        id: loader
        objectName: "loader"
        active: false
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", {"canary": 3});
        root.initialValue = loader.item.canary; // error - item should not yet exist.
    }
}
