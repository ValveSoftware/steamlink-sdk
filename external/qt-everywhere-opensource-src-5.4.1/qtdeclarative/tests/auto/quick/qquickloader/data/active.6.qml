import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"

        property int activeChangedCount: 0
        onActiveChanged: activeChangedCount = activeChangedCount + 1
    }

    function doSetActive() {
        loader.active = true;
    }

    function doSetInactive() {
        loader.active = false;
    }
}
