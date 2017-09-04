import QtQuick 2.0

import Qt.test 1.0 as QtTest                                      // singleton Type installed into existing uri
import Qt.test.qobjectApiParented 1.0 as QtTestParentedQObjectApi // qobject (with parent) singleton Type installed into a new uri

QtObject {
    property int existingUriTest: QtTest.QObject.qobjectTestWritableProperty
    property int qobjectParentedTest: QtTestParentedQObjectApi.QObject.qobjectTestWritableProperty

    function modifyValues() {
        QtTest.QObject.qobjectTestWritableProperty = 50;
        QtTestParentedQObjectApi.QObject.qobjectTestWritableProperty = 65;
    }
}

