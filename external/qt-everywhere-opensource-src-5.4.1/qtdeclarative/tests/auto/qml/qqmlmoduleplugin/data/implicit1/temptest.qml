import QtQuick 2.0

// this qml file uses a type which is meant to be defined
// in a plugin which is specified in the qmldir file.
// however, that plugin doesn't exist, so it cannot be
// loaded, and hence the AItem type will be an unknown type.

Item {
    id: root

    AItem {
        id: unknown
    }
}
