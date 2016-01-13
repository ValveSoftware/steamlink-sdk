import QtQuick 2.0

Loader {
    active: false
    function setSourceComponent() {
        sourceComponent = Qt.createComponent("SimpleTestComponent.qml");
    }
}
