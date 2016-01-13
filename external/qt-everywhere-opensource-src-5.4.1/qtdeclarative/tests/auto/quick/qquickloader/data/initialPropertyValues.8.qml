import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0

    Loader {
        id: loader
        objectName: "loader"
        active: false
        onLoaded: {
            root.initialValue = loader.item.canary;         // should be six
        }
    }

    Component.onCompleted: {
        loader.setSource("http://127.0.0.1:14458/InitialPropertyValuesComponent.qml", {"canary": 6});
        loader.active = true;
    }
}
