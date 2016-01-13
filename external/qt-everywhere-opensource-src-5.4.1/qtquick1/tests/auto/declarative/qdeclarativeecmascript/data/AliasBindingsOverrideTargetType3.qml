import QtQuick 1.0

QtObject {
    id: root

    property int testProperty
    property alias aliasProperty: root.testProperty
}

