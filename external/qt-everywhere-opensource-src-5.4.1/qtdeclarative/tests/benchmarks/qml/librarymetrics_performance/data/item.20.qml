import QtQuick 2.0

Item {
    Item {
        id: c1
        function someFunc() { return 42; }
        property int b: someFunc()
    }
    property alias c1bAlias: c1.b

    property int a: c1bAlias * 5
}
