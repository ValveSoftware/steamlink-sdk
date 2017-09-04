import QtQuick 2.0

Item {
    property bool success: false
    property var syncComponent
    property var asyncComponent

    function asyncStatusChanged() {
        if (asyncComponent.status == Component.Ready && syncComponent.status == Component.Ready) {
            success = true;
            var ao = asyncComponent.createObject();
            var so = syncComponent.createObject();
            if (ao.c1one != 6) success = false;
            if (so.c1one != 55) success = false;
            ao.destroy();
            so.destroy();
        }
    }

    Component.onCompleted: {
        asyncComponent = Qt.createComponent("TestComponent.2.qml", Component.Asynchronous);
        if (asyncComponent.status != Component.Loading)
            return;
        asyncComponent.statusChanged.connect(asyncStatusChanged);
        syncComponent = Qt.createComponent("TestComponent.3.qml", Component.PreferSynchronous);
    }
}
