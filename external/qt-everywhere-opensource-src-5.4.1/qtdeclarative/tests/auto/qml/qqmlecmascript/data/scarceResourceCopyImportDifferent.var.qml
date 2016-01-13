import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceCopyImportDifferent.var.js" as ScarceResourceCopyImportJs

// in this case, the ScarceResourceCopyImportJs returns a _new_, different
// scarce resource each time.  Invalidating one will not invalidate the others.

QtObject {
    // this binding is evaluated once, prior to the resource being released
    property var scarceResourceImportedCopy: ScarceResourceCopyImportJs.importScarceResource()

    // the following properties are assigned on component completion.
    property bool arePropertiesEqual
    property var scarceResourceAssignedCopyOne;
    property var scarceResourceAssignedCopyTwo;
    Component.onCompleted: {
        scarceResourceAssignedCopyOne = ScarceResourceCopyImportJs.importScarceResource();
        arePropertiesEqual = (scarceResourceAssignedCopyOne != scarceResourceImportedCopy); // they're not the same object.
        ScarceResourceCopyImportJs.destroyScarceResource(); // makes the MOST RECENT resource invalid (ie, assignedCopyOne).
        scarceResourceAssignedCopyTwo = ScarceResourceCopyImportJs.importScarceResource();
    }
}