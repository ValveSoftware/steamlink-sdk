import QtQuick 2.0
import Qt.test 1.0

// Here we import a scarce resource directly.
// The instance has a property which is a copy
// of the scarce resource, so it should not be
// detached (but we should automatically release
// the resource from our engine internal list).

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property variant scarceResourceCopy: scarceResourceProvider.scarceResource
}

