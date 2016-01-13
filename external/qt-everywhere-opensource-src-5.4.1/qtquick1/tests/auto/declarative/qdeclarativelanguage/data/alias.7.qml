import QtQuick 1.0

QtObject {
    property QtObject object
    property alias aliasedObject: target.object

    object: QtObject {
        id: target

        property QtObject object
        object: QtObject {}
    }
}

