import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceMultipleDifferentNoBinding.var.js" as ScarceResourcesMultipleDifferentNoBinding

QtObject {
    property var resourceOne
    property var resourceTwo

    Component.onCompleted: {
        resourceOne = ScarceResourcesMultipleDifferentNoBinding.importScarceResource();
        resourceTwo = ScarceResourcesMultipleDifferentNoBinding.importScarceResource();
        ScarceResourcesMultipleDifferentNoBinding.releaseScarceResource(resourceTwo);
    }
}