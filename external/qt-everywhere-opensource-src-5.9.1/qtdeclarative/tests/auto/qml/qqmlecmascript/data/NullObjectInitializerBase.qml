import QtQml 2.0
QtObject {
    property Component factory: Component { QtObject {} }
    property QtObject testProperty: factory.createObject()
}
