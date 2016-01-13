import QtQuick 2.0

Item {
    id: root
    objectName: "root"
    function generate() {
        var f = null;
        if (root.objectName == "root") {
            f = function(param) { return 42 - param; }
        } else {
            f = function(param) { return 9000 * param; }
        }
        return f;
    }

    property int b: 50
    property var a: b + generate()()
}
