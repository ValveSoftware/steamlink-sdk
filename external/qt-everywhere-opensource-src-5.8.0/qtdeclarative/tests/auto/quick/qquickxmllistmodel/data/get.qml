import QtQuick 2.0
import QtQuick.XmlListModel 2.0

XmlListModel {
    source: "model.xml"
    query: "/Pets/Pet"
    XmlRole { name: "name"; query: "name/string()" }
    XmlRole { name: "type"; query: "type/string()" }
    XmlRole { name: "age"; query: "age/number()" }
    XmlRole { name: "size"; query: "size/string()" }

    id: root

    property bool preTest: false
    property bool postTest: false

    function runPreTest() {
        if (root.get(0) != undefined)
            return;

        preTest = true;
    }

    function runPostTest() {
        if (root.get(-1) != undefined)
            return;

        var row = root.get(0);
        if (row.name != "Polly" ||
            row.type != "Parrot" ||
            row.age != 12 ||
            row.size != "Small")
            return;

        row = root.get(1);
        if (row.name != "Penny" ||
            row.type != "Turtle" ||
            row.age != 4 ||
            row.size != "Small")
            return;

        row = root.get(7);
        if (row.name != "Rover" ||
            row.type != "Dog" ||
            row.age != 0 ||
            row.size != "Large")
            return;

        row = root.get(8);
        if (row.name != "Tiny" ||
            row.type != "Elephant" ||
            row.age != 15 ||
            row.size != "Large")
            return;

        if (root.get(9) != undefined)
            return;

        postTest = true;
    }
}
