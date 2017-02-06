import QtQuick 2.0

Item {
    id: root
    property bool returnValue: false

    property string first
    property string second
    property string third
    property string fourth
    property string fifth
    property string sixth
    property string seventh
    property string eighth
    property string ninth

    function success() {
        var a = "Value is %1";
        for (var ii = 0; ii < 10; ++ii) {
            first = a.arg("string");
            second = a.arg(1);
            third = a.arg(true);
            fourth = a.arg(3.345);
            fifth = a.arg(undefined);
            sixth = a.arg(null);
            seventh = a.arg({"test":5});
            eighth = a.arg({"test":5, "again":6});
        }

        if (first != "Value is string") returnValue = false;
        if (second != "Value is 1") returnValue = false;
        if (third != "Value is true") returnValue = false;
        if (fourth != "Value is 3.345") returnValue = false;
        if (fifth != "Value is undefined") returnValue = false;
        if (sixth != "Value is null") returnValue = false;
        if (seventh != "Value is [Object object]") returnValue = false;
        if (eighth != "Value is [Object object]") returnValue = false;
        returnValue = true;
    }

    function failure() {
        returnValue = true;
        var a = "Value is %1";
        for (var ii = 0; ii < 10; ++ii) {
            ninth = a.arg(1,2,3,4);
        }
        returnValue = false;
    }
}
