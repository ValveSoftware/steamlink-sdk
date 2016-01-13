import QtQuick 2.0
import Qt.test 1.0

Item {
    id: first
    property var vp: Item {
        id: second
        property MyScarceResourceObject srp;
        srp: MyScarceResourceObject { id: scarceResourceProvider }
        property var sr: scarceResourceProvider.scarceResource
        property var canary: 5
    }
}
