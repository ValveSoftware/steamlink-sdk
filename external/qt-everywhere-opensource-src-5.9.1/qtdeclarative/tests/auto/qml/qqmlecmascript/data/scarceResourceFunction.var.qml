import QtQuick 2.0
import Qt.test 1.0

// Here we import a scarce resource directly.
// The copy is only assigned when retrieveScarceResource()
// is called, and so should be detached prior to that.
// The copy should be released when releaseScarceResource()
// is called, and so should be detached after that.

QtObject {
    id: root
    property MyScarceResourceObject a: MyScarceResourceObject { id: scarceResourceProvider }
    property var scarceResourceCopy;

    function retrieveScarceResource() {
        root.scarceResourceCopy = scarceResourceProvider.scarceResource;
    }

    function releaseScarceResource() {
        root.scarceResourceCopy = undefined;
    }
}

