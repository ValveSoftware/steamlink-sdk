import QtQuick 2.0
import Qt.test.qobjectApi 1.0 as QObjectApi

Item {
    property bool success: true
    property Item testProp: null

    // first we create an object and reference it via a dynamic variant property
    function createReference() {
        var c = Qt.createComponent("HRMDPComponent.qml");
        testProp = c.createObject(null); // QML ownership.
    }

    // after a gc, it should not have been collected.
    function ensureReference() {
        if (testProp == null) success = false;            // should not have triggered delete notify / zeroed testProp value
        if (testProp.variantCanary != 5) success = false; // should not have deleted vmemo of object referenced by testProp
        if (testProp.varCanary != 12) success = false;    // should not have collected vmemo vmeProperties
        if (QObjectApi.QObject.qobjectTestWritableProperty != 42) success = false; // should not have been set to 43.
    }

    // then we manually delete the item being referenced
    function manuallyDelete() {
        QObjectApi.QObject.deleteQObject(testProp);
        if (QObjectApi.QObject.qobjectTestWritableProperty != 43) success = false; // should have been set to 43.
    }

    // after a gc (and deferred deletion process) the object should be gone
    function ensureDeleted() {
        // a crash should not have occurred during the previous gc due to the
        // VMEMO attempting to keep a previously deleted QObject alive.
        if (testProp != null) success = false; // delete notify should have zeroed testProp value.
    }
}
