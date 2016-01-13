import QtQuick 2.0

QtObject {
    id: root

    property alias aliasProperty: root.realProperty
    onAliasPropertyChanged: root.test = true

    property int realProperty: 0

    property bool test: false

    Component.onCompleted: {
        root.realProperty = 10;
    }
}
