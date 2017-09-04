import QtQml 2.0

QtObject {
    id: screen

    property Connections c1: Connections { objectName: "connections"; target: screen; onFooBar: {} }
}
