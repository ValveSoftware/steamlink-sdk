import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    function loadComponents() {
        asyncLoader.source = "TestComponent.2.qml"
        syncLoader.source = "TestComponent.qml"
    }

    Loader {
        id: asyncLoader
        objectName: "asyncLoader"
        asynchronous: true
    }

    Loader {
        id: syncLoader
        objectName: "syncLoader"
        asynchronous: false
    }
}
