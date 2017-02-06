import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    id: root
    property bool success: true
    property int value: 12345

    Timer {
        interval: 0
        repeat: false
        running: true

        onTriggered: {
            var factory = Qt.createComponent("dynamicEntity.qml");
            var dynamicChild = factory.createObject(root, { "value": 27182818 });
        }
    }
}
