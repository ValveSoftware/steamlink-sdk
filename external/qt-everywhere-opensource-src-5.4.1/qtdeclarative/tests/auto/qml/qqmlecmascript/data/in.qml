import QtQuick 2.0

Item {
    id: root
    property bool test1: "x" in root
    property bool test2: !("foo" in root)
}
