import QtQuick 2.0

QtObject {
    property list<QtObject> objects
    objects: [QtObject { objectName: "obj1" }, QtObject { objectName: "obj2" }, QtObject { objectName: "obj3" }]
    property string listResult

    function listProperty() {
        for (var i in objects)
            listResult += i + "=" + objects[i].objectName + "|"
    }
}

