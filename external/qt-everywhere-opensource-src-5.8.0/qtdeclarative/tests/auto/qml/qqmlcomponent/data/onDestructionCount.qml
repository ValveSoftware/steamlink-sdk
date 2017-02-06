import QtQuick 2.0

Item {
    Component {
        id: internalComponent

        Item {
            Component.onDestruction: console.warn('Component.onDestruction')
        }
    }

    Component.onCompleted: {
        internalComponent.createObject()
        gc()
    }
}
