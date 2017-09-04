import QtQuick 2.6
import "simpleimportByName"

Item {
    Component.onCompleted: {
        console.warn(SimpleType)
    }
}

