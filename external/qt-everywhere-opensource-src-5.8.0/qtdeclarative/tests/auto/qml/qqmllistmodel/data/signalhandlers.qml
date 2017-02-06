import QtQuick 2.0

ListModel{
    property bool ok: false
    property int foo: 5
    onFooChanged: ok = true
    Component.onCompleted: foo = 6
}
