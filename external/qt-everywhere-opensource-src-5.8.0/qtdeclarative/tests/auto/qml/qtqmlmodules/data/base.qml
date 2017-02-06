import QtQml 2.0

QtObject {
    property bool success: {
        prop1 != undefined &&
        prop2 != undefined &&
        prop3 != undefined &&
        prop4 != undefined
    }
    property Component prop1: Component { QtObject {}}
    property Timer prop2: Timer {}
    property Connections prop3: Connections{}
    property Binding prop4: Binding{}
}
