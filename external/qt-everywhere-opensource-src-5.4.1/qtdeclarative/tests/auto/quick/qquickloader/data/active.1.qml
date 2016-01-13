import QtQuick 2.0

Item {
    id: root

    Loader {
        id: loader
        objectName: "loader"
        active: false
    }

    Component {
        id: inlineTestComponent
        Item {
            id: inlineTestItem
            property int someProperty: 5
        }
    }

    function doSetSource() {
        loader.source = "ActiveComponent.qml";
    }

    function doSetSourceComponent() {
        loader.sourceComponent = inlineTestComponent;
    }

    function doSetActive() {
        loader.active = true;
    }
}
