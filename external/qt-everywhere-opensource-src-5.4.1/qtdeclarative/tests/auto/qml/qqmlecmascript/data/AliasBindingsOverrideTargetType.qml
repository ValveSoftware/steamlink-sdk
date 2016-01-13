import QtQuick 2.0
import Qt.test 1.0

MyTypeObject {
    id: root

    property int data: 7

    property int targetProperty: root.data * 43 - root.data
    property alias aliasProperty: root.targetProperty

    pointProperty: Qt.point(data, data);
    property alias pointAliasProperty: root.pointProperty
}
