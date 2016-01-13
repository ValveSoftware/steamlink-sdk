import QtQuick 2.0

Item {
    id: root
    property int loaderValue: 0
    property int createObjectValue: 0

    Loader {
        id: loader
        objectName: "loader"
        onLoaded: {
            root.loaderValue = loader.item.canary; // should still be one
        }
    }

    Item {
        id: child
        property int bindable: 1;
    }

    property InitialPropertyValuesComponent ipvc
    Component.onCompleted: {
        loader.setSource("InitialPropertyValuesComponent.qml", {"canary": child.bindable});
        var dynComp = Qt.createComponent("InitialPropertyValuesComponent.qml");
        ipvc = dynComp.createObject(root, {"canary": child.bindable});
        child.bindable = 7; // won't cause re-evaluation, since not used in a binding.
        root.createObjectValue = ipvc.canary;   // should still be one
    }
}
