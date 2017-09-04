import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceCopyImport.var.js" as ScarceResourceCopyImportJs

QtObject {
    // this binding is evaluated once, prior to the resource being released
    property var scarceResourceImportedCopy: ScarceResourceCopyImportJs.importScarceResource()

    property bool arePropertiesEqual
    property var scarceResourceAssignedCopyOne;
    property var scarceResourceAssignedCopyTwo;
    Component.onCompleted: {
        scarceResourceAssignedCopyOne = ScarceResourceCopyImportJs.importScarceResource();
        arePropertiesEqual = (scarceResourceAssignedCopyOne == scarceResourceImportedCopy);
        ScarceResourceCopyImportJs.destroyScarceResource(); // makes all properties invalid.
        scarceResourceAssignedCopyTwo = ScarceResourceCopyImportJs.importScarceResource();
    }
}