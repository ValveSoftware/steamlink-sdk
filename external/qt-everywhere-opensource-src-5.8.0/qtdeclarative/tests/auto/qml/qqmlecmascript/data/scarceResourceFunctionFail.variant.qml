import QtQuick 2.0
import Qt.test 1.0

// In this example, a common syntax error will only be "caught"
// when the function is called via:
// QQmlVMEMetaObject::metaCall->invokeMetaMethod()
// We would like to ensure that a useful error message is printed.

QtObject {
    id: root
    property MyScarceResourceObject a: MyScarceResourceObject { id: scarceResourceProvider }
    property variant scarceResourceCopy;
    property string srp_name: a.toString();

    function retrieveScarceResource() {
        root.scarceResourceCopy = scarceResourceProvider.scarceResource(); // common syntax error, should throw exception
    }

    function releaseScarceResource() {
        root.scarceResourceCopy = null;
    }
}

