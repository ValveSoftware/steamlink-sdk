import QtQuick 2.0

Item {
    id: root

    property real test: 9
    property real test2: 3

    property alias aliasProperty: innerObject.realProperty

    property QtObject innerObject: QtObject {
        id: innerObject
        property real realProperty: test * test + test
    }

    states: State {
        name: "switch"
        PropertyChanges {
            target: root
            aliasProperty: 32 * test2
        }
    }
}
