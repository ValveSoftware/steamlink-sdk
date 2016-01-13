import QtQuick 1.0

Item {
    id: root

    property real test: 9
    property real test2: 3

    property real realProperty: test * test + test
    property alias aliasProperty: root.realProperty

    states: State {
        name: "switch"
        PropertyChanges {
            target: root
            aliasProperty: 32 * test2
        }
    }
}
