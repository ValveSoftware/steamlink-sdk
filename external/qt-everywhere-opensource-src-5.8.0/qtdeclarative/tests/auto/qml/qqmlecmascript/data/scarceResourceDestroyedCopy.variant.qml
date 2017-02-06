import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceTest.variant.js" as ScarceResourceProviderJs

// In this case, following the evaluation of the binding,
// the scarceResourceTest value should be an invalid variant,
// since the scarce resource will have been released.

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property variant scarceResourceCopy: ScarceResourceProviderJs.importReleasedScarceResource(scarceResourceProvider);
}

