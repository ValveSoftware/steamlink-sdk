import QtQuick 2.0

QtObject {
    property int a: 10
    property int b: 10 + a
    property QtObject c: QtObject {}
    property list<QtObject> d: [ QtObject {}, QtObject {} ]
}
