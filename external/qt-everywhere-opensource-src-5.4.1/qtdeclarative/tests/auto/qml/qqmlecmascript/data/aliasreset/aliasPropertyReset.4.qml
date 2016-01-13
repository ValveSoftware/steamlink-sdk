import QtQuick 2.0
import Qt.test 1.0

Item {
    id: first
    property alias sourceComponentAlias: loader.sourceComponent

    Component {
        id: redSquare
        Rectangle { color: "red"; width: 10; height: 10 }
    }

    Loader {
        id: loader
        objectName: "loader"
        sourceComponent: redSquare
    }

    function resetAlias() {
        sourceComponentAlias = undefined; // ensure we don't crash after deletion of loader.
    }

    function setAlias() {
        sourceComponentAlias = redSquare;
    }
}
