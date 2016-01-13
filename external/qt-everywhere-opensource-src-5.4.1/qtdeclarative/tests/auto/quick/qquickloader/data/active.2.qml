import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"
        source: "ActiveComponent.qml";

        property int statusChangedCount:  0
        onStatusChanged: statusChangedCount = statusChangedCount + 1
    }

    function doSetInactive() {
        loader.active = false;
    }
}
