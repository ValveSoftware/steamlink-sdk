import QtQuick 2.0

Item {
    Loader {
        source: "PropertyVarBaseItem.qml"
        onLoaded: item.destroy()
    }
    Loader {
        Component.onCompleted: setSource("PropertyVarBaseItem.qml", { random: "" })
        onLoaded: item.destroy()
    }

    Repeater {
        model: 1
        Item { Component.onCompleted: destroy() }
    }
    Repeater {
        model: 1
        Item { id: me; Component.onCompleted: { setObject(me); getObject().destroy() } }
    }
}
