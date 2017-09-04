import QtQuick 2.6
import "simpleimportByName" as ImportName

Item {
    Component.onCompleted: {
        console.warn(ImportName.SimpleType)
    }
}

