import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"
    }

    Component.onCompleted: {
        loader.setSource("InvalidSourceComponent.qml", {"canary":3});
    }
}
