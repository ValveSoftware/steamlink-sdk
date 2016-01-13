import QtQuick 1.0

Item {
    function clear() {
        loader.sourceComponent = undefined
    }
    Component { id: comp; Rectangle { width: 100; height: 50 } }
    Loader { id: loader; sourceComponent: comp }
}
