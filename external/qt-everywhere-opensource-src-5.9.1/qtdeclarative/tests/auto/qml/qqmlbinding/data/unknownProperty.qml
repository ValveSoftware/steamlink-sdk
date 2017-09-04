import QtQuick 2.0

Item {
    id: root

    Binding {
        target: root
        property: "unknown"
        value: 42
    }
}
