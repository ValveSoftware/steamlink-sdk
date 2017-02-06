import QtQuick 2.0

Item {
    id: root

    readonly property string name: "John"

    Binding {
        target: root
        property: "name"
        value: "Doe"
        when: false
    }
}
