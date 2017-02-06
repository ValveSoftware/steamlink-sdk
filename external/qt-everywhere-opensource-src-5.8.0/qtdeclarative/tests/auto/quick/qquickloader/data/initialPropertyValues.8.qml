import QtQuick 2.0

Item {
    id: root
    property int initialValue: 0
    property string serverBaseUrl;

    Loader {
        id: loader
        objectName: "loader"
        active: false
        onLoaded: {
            root.initialValue = loader.item.canary;         // should be six
        }
    }

    Component.onCompleted: {
        loader.setSource(serverBaseUrl + "/InitialPropertyValuesComponent.qml", {"canary": 6});
        loader.active = true;
    }
}
