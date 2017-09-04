import QtQml 2.0

QtObject {
    id: root
    property alias topLevelAlias: subObject.targetProperty

    property QtObject referencingSubObject: QtObject {
        property alias success: root.topLevelAlias
    }

    property QtObject foo: QtObject {
        id: subObject
        property bool targetProperty: true
    }
}
