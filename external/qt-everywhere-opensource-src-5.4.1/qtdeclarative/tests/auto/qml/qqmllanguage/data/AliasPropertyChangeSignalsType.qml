import QtQuick 2.0

QtObject {
    id: root

    signal sig1
    signal sig2
    signal sig3
    signal sig4

    property alias aliasProperty: root.realProperty

    property int realProperty: 0

    property bool test: false

    Component.onCompleted: {
        root.realProperty = 10;
    }
}
