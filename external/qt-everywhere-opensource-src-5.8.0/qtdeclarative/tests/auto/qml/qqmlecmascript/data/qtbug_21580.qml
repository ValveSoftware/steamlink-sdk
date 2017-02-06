import QtQuick 2.0

QtObject {
    property bool test: false

    property list<QtObject> objects: [
        QtObject {
            id: first
            property alias myAlias: other.myProperty
            onMyAliasChanged: if (myAlias == 20) test = true
        },
        QtObject {
            id: other
            property real myProperty
        }
    ]

    Component.onCompleted: {
        other.myProperty = 20;
    }
}

