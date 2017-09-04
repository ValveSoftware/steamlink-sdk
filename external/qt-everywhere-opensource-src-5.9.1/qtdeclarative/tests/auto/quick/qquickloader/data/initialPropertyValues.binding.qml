import QtQuick 2.0

Item {
    id: root

    property InitialPropertyValuesComponent testInstance
    testInstance: loader.item

    property int bindable:  1
    property int canaryValue: testInstance.canary
    property int behaviorCount: testInstance.behaviorCount

    Loader {
        id: loader
        objectName: "loader"
    }

    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", {"canary": Qt.binding(function() { return root.bindable })});
    }
}
