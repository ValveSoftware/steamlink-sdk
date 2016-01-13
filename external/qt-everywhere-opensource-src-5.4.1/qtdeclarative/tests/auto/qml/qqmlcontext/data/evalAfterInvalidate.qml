import QtQuick 2.0

Rectangle {
    id: root

    Component.onCompleted: {
        var i = containerComponent.createObject(root);
        contentComponent.createObject(i);
        i.destroy()
    }

    property Component containerComponent: ContainerComponent {}
    property Component contentComponent: ContentComponent {}
}
