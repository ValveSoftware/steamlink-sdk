import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceTest.var.js" as ScarceResourceProviderJs

// In this case, following the evaluation of the binding,
// the scarceResourceTest value should be an invalid variant,
// since the scarce resource will have been released.

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property var scarceResourceCopy: ScarceResourceProviderJs.importReleasedScarceResource(scarceResourceProvider);
}