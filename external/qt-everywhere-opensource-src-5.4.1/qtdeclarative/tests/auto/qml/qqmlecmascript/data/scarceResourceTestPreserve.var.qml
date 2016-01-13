import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceTest.var.js" as ScarceResourceProviderJs

// In this case, the scarce resource is explicitly preserved.
// It should not be automatically released after the evaluation
// of the binding is complete, but instead will be kept in
// memory until the JS garbage collector runs.

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property int scarceResourceTest: ScarceResourceProviderJs.importPreservedScarceResource(scarceResourceProvider),100 // return 100, but the resource should be preserved.
}

