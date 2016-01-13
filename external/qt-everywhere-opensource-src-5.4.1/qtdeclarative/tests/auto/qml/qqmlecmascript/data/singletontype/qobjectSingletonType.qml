import QtQuick 2.0

import Qt.test 1.0 as QtTest                                    // singleton Type installed into existing uri
import Qt.test.qobjectApi 1.0 as QtTestQObjectApi               // qobject singleton Type installed into new uri
import Qt.test.qobjectApi 1.3 as QtTestMinorVersionQObjectApi   // qobject singleton Type installed into existing uri with new minor version
import Qt.test.qobjectApi 2.0 as QtTestMajorVersionQObjectApi   // qobject singleton Type installed into existing uri with new major version
import Qt.test.qobjectApiParented 1.0 as QtTestParentedQObjectApi // qobject (with parent) singleton Type installed into a new uri

QtObject {
    property int existingUriTest: QtTest.QObject.qobjectTestProperty
    property int qobjectTest: QtTestQObjectApi.QObject.qobjectTestProperty
    property int qobjectMethodTest: 3
    property int qobjectMinorVersionMethodTest: 3
    property int qobjectMinorVersionTest: QtTestMinorVersionQObjectApi.QObject.qobjectTestProperty
    property int qobjectMajorVersionTest: QtTestMajorVersionQObjectApi.QObject.qobjectTestProperty
    property int qobjectParentedTest: QtTestParentedQObjectApi.QObject.qobjectTestProperty

    Component.onCompleted: {
        qobjectMethodTest = QtTestQObjectApi.QObject.qobjectTestMethod(); // should be 1
        qobjectMethodTest = QtTestQObjectApi.QObject.qobjectTestMethod(); // should be 2
        qobjectMinorVersionMethodTest = QtTestMinorVersionQObjectApi.QObject.qobjectTestMethod(); // should be 1
    }
}

