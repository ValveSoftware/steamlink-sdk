import QtQml 2.0

QtObject {
    property Connections c1: Connections { objectName: "connections"; target: null; onNotFooBar: {} }
}
