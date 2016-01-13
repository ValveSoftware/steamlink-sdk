import QtQuick 2.0

// this qml file attempts to import an invalid version of a qobject singleton Type.

import Qt.test.qobjectApi 4.0 as QtTestMajorVersionQObjectApi // qobject singleton Type installed into existing uri with nonexistent major version

QtObject {
    property int qobjectMajorVersionTest: QtTestMajorVersionQObjectApi.qobjectTestProperty
}

