import QtQml 2.0

QtObject {
    id: root
    property alias a: subObject.b
    property QtObject foo: QtObject {
        id: subObject
        property alias b: root.a
    }
}
