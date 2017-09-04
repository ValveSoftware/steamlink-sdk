import QtQuick 2.0

Item {
    property int a: 0
    function someFunc() {
        var tmp = 5;
        var another = tmp * 5 + 3;
        var yetanother = another % 5;
        if (yetanother > 2) a = 42;
        else a = 9000;
        return 42;
    }
}
