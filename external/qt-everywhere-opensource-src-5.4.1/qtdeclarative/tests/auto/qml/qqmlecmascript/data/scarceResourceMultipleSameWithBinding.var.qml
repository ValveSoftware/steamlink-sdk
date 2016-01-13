import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceMultipleDifferentNoBinding.var.js" as ScarceResourcesMultipleDifferentNoBinding

QtObject {
    property var resourceOne: ScarceResourcesMultipleDifferentNoBinding.importScarceResource()
    property var resourceTwo: resourceOne

    Component.onCompleted: {
        ScarceResourcesMultipleDifferentNoBinding.releaseScarceResource(resourceTwo);
    }
}