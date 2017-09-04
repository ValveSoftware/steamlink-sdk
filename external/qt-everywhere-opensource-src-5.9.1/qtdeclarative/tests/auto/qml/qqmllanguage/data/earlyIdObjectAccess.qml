import QtQuick 2.0

Item {
    visible: false
    property bool success: false
    property bool loading: true

    Item {
        visible: someOtherItem.someProperty
        onVisibleChanged: {
            if (!visible) {
                success = loading
            }
        }
    }

    Item {
        id: someOtherItem
        property bool someProperty: true
    }

    Component.onCompleted: loading = false
}
