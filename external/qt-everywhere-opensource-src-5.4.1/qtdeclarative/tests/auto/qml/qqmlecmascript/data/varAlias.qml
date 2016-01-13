import QtQuick 2.0

QtObject {
    id: root

    property int test: root.aliasProperty.value
    property alias aliasProperty: root.varProperty
    property var varProperty: new Object({ value: 192 });
}
