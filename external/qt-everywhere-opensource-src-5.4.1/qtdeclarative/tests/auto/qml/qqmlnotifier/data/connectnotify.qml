import QtQuick 2.0
import Test 1.0

Item {
    id: root
    ExportedClass {
        id: exportedClass
        objectName: "exportedClass"
        onBoundSignal: {}
    }

    property int v4Binding: exportedClass.v4BindingProp

    property int v8Binding: {
        Math.abs(12); // Prevent optimization to v4
        return exportedClass.v8BindingProp
    }

    property int scriptBinding: {
        function innerFunction() {} // Prevent usage of v4 or v8 bindings
        return exportedClass.scriptBindingProp
    }

    property int foo: exportedClass.qmlObjectProp
    property int baz: _exportedObject.cppObjectProp

    // v4 bindings that could share a subscription. They don't, though, and the code
    // relies on that
    property int v4Binding2: exportedClass.v4BindingProp2
    property int bla: exportedClass.v4BindingProp2

    function removeV4Binding() {
        //console.log("Going to remove v4 binding...")
        root.v4Binding = 1;
        //console.log("Binding removed!")
    }

    function removeV8Binding() {
        //console.log("Going to remove v8 binding...")
        root.v8Binding = 1;
        //console.log("Binding removed!")
    }

    function removeScriptBinding() {
        //console.log("Going to remove script binding...")
        root.scriptBinding = 1;
        //console.log("Binding removed!")
    }

    function removeV4Binding2() {
        //console.log("Going to remove v4 binding 2...")
        root.v4Binding2 = 1;
        //console.log("Binding removed!")
    }

    function readProperty() {
        var test = exportedClass.unboundProp
    }

    function changeState() {
        //console.log("Changing state...")
        if (root.state == "") root.state = "state1"
        else                  root.state = ""
        //console.log("State changed.")
    }

    property int someValue: 42

    states: State {
        name: "state1"
        PropertyChanges { target: root; someValue: exportedClass.unboundProp }
    }
}
