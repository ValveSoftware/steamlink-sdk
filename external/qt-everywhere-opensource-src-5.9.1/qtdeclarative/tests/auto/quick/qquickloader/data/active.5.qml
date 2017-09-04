import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"
        source: "ActiveComponent.qml";

        property int itemChangedCount:  0
        onItemChanged: itemChangedCount = itemChangedCount + 1
    }

    function doSetInactive() {
        loader.active = false;
    }
}
