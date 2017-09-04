import QtQuick 2.0

Item {
    id: root

    property QtObject incubatedItem

    Component.onCompleted: {
        var component = Qt.createComponent("PropertyVarBaseItem.qml");

        var incubator = component.incubateObject(root);
        if (incubator.status != Component.Ready) {
            incubator.onStatusChanged = function(status) {
                if (status == Component.Ready) {
                    incubatedItem = incubator.object;
                }
            }
        } else {
            incubatedItem = incubator.object;
        }
    }

    function deleteIncubatedItem() {
        incubatedItem.destroy();
        gc();
    }
}
