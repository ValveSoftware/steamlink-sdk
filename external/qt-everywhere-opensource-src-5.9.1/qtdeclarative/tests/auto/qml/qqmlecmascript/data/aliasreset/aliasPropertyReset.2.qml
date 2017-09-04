import QtQuick 2.0
import Qt.test 1.0

Item {
    id: first
    property bool loaderSourceComponentIsUndefined: false
    property alias sourceComponentAlias: loader.sourceComponent

    Component {
        id: redSquare
        Rectangle { color: "red"; width: 10; height: 10 }
    }

    Loader {
        id: loader
        sourceComponent: redSquare
    }

    function resetAlias() {
        sourceComponentAlias = undefined;
        loaderSourceComponentIsUndefined = (loader.sourceComponent == undefined);
    }
}
