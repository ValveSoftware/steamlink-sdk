import QtQuick 1.0

Item {
    id: root
    property bool test1: "x" in root
    property bool test2: !("foo" in root)
}
