import QtQuick 2.0

QtObject {
    function checkPropertyDeletion() {
        var o = {
            x: 1,
            y: 2
        };
        o.z = 3
        delete o.y;

        return (o.x === 1 && o.y === undefined && o.z === 3)
    }

    property bool result: checkPropertyDeletion()
}
