import QtQuick 2.0

Item {
    id: root

    Component.onCompleted: {
        var container = containerComponent.createObject(root)
        contentComponent.createObject(container)
        container.destroy();

        pendingEvents.process()
    }

    property Component containerComponent: ContainerComponent {}
    property Component contentComponent: ContentComponent {}
}

