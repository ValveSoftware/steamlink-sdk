import QtQuick 2.0

// this qml file attempts to import an invalid version of a qobject singleton Type.

import Qt.test.qobjectApi 1.7 as QtTestMinorVersionQObjectApi // qobject singleton Type installed into existing uri with nonexistent minor version

QtObject {
    property int qobjectMinorVersionTest: QtTestMinorVersionedQObjectApi.qobjectTestProperty
}

