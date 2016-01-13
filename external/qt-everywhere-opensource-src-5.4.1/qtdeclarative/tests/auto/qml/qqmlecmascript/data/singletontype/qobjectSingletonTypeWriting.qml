import QtQuick 2.0
import Qt.test 1.0 as QtTest     // qobject singleton Type installed into existing uri

QtObject {
    property int firstProperty: 1
    property int secondProperty: 2
    property int readOnlyProperty: QtTest.QObject.qobjectTestProperty
    property int writableProperty: QtTest.QObject.qobjectTestWritableProperty
    property int writableFinalProperty: QtTest.QObject.qobjectTestWritableFinalProperty

    onFirstPropertyChanged: {
        // In this case, we want to attempt to set the singleton Type property.
        // This should fail, as the singleton Type property is read only.
        if (firstProperty != QtTest.QObject.qobjectTestProperty) {
            QtTest.QObject.qobjectTestProperty = firstProperty; // should silently fail.
        }
    }

    onSecondPropertyChanged: {
        // In this case, we want to attempt to set the singleton Type properties.
        // This should succeed, as the singleton Type properties are writable.
        if (secondProperty != QtTest.QObject.qobjectTestWritableProperty) {
            QtTest.QObject.qobjectTestWritableProperty = secondProperty; // should succeed.
        }
        if (secondProperty != QtTest.QObject.qobjectTestWritableFinalProperty) {
            QtTest.QObject.qobjectTestWritableFinalProperty = secondProperty; // should succeed.
        }
    }
}

