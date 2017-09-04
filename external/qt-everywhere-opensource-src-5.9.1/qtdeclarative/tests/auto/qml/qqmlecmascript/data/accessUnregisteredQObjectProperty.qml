import QtQuick 2.4
import Test 1.0

Item {
    property alias container: container

    ObjectContainer {
        id: container
    }

    function readProperty() {
        var v = container.object1;
    }

    function writeProperty() {
        container.object2 = null;
    }
}

