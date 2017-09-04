import QtQuick 2.0

Item {
    id: root

    property Component comp: OnDestructionComponent {
        property int b: 50
        onParentChanged: b += a
    }

    property Item compInstance

    Component.onCompleted: {
        compInstance = comp.createObject(root);
        compInstance.destroy();
    }
}

