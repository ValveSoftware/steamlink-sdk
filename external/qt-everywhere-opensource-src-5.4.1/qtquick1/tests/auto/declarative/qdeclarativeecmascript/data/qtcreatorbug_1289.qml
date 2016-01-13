import QtQuick 1.0

QtObject {
    id: root
    property QtObject object: QtObject {
        id: nested
        property QtObject nestedObject
    }

    Component.onCompleted: {
        nested.nestedObject = root;
    }
}
