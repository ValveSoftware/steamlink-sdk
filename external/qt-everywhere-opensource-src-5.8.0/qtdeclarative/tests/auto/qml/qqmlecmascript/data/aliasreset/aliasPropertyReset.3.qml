import QtQuick 2.0
import Qt.test 1.0

Item {
    id: first
    property bool loaderTwoSourceComponentIsUndefined: false
    property bool loaderOneSourceComponentIsUndefined: false
    property alias sourceComponentAlias: loaderOne.sourceComponent

    Component {
        id: redSquare
        Rectangle { color: "red"; width: 10; height: 10 }
    }

    Loader {
        id: loaderOne
        sourceComponent: loaderTwo.sourceComponent
    }

    Loader {
        id: loaderTwo
        sourceComponent: redSquare
        x: 15
    }

    function resetAlias() {
        sourceComponentAlias = undefined; // loaderOne.sourceComponent should be set to undefined instead of l2.sc
        loaderOneSourceComponentIsUndefined = (loaderOne.sourceComponent == undefined); // should be true
        loaderTwoSourceComponentIsUndefined = (loaderTwo.sourceComponent == undefined); // should be false
    }
}
