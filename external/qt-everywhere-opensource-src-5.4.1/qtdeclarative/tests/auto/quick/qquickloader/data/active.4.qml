import QtQuick 2.0

Item {
    id: root

    Component {
        id: inlineTestComponent
        Item {
            id: inlineTestItem
            property int someProperty: 5
        }
    }

    Loader {
        id: loader
        objectName: "loader"
        sourceComponent: inlineTestComponent

        property int sourceComponentChangedCount:  0
        onSourceComponentChanged: sourceComponentChangedCount = sourceComponentChangedCount + 1
    }

    function doSetInactive() {
        loader.active = false;
    }
}
