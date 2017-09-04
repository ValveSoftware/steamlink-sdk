import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceTest.var.js" as ScarceResourceProviderJs

// Here we import a scarce resource directly, from JS module.
// It is not preserved or released manually, so it should be
// automatically released once evaluation of the binding
// is complete.

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property var scarceResourceCopy: ScarceResourceProviderJs.importScarceResource(scarceResourceProvider)
}