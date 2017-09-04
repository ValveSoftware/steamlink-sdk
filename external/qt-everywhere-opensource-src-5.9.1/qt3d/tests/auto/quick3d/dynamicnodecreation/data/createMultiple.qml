import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    property bool success: true
    property int value: 12345

    Entity {
        objectName: "childEntity"
        property bool success: false
        property int value: 54321
    }
}
