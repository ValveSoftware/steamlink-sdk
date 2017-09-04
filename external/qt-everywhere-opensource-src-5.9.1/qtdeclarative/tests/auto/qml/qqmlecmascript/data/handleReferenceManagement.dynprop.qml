import QtQuick 2.0
import Qt.test.qobjectApi 1.0 as QObjectApi

Item {
    property bool success: true
    property variant testProp: null

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

    // then we remove the reference.
    function removeReference() {
        testProp = null; // allow original object to be released.
    }

    // after a gc (and deferred deletion process) the object should be gone
    function ensureDeletion() {
        if (QObjectApi.QObject.qobjectTestWritableProperty != 43) success = false; // should have been set to 43.
    }
}
