import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"
        source: "ActiveComponent.qml";

        property int sourceChangedCount:  0
        onSourceChanged: sourceChangedCount = sourceChangedCount + 1
    }

    function doSetInactive() {
        loader.active = false;
    }
}
