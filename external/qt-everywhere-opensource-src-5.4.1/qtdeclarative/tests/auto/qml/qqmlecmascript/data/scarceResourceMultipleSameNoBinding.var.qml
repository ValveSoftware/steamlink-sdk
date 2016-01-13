import QtQuick 2.0
import Qt.test 1.0
import "scarceResourceMultipleSameNoBinding.var.js" as ScarceResourcesMultipleSameNoBinding

QtObject {
    property var resourceOne
    property var resourceTwo

    Component.onCompleted: {
        resourceOne = ScarceResourcesMultipleSameNoBinding.importScarceResource();
        resourceTwo = ScarceResourcesMultipleSameNoBinding.importScarceResource();
        ScarceResourcesMultipleSameNoBinding.releaseScarceResource(resourceTwo);
    }
}