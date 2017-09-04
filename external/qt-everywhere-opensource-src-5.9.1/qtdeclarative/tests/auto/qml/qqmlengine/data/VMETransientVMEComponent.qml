import QtQuick 2.0

Item {
    property var p: null

    Component.onCompleted: {
        var c = Qt.createComponent('VMEComponent.qml')
        p = c.createObject()
        c.destroy()
    }

    Component.onDestruction: {
        p.destroy()
    }
}
