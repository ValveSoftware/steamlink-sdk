import QtQuick 1.0

QtObject {
    property list<QtObject> listProperty
    property int test: listProperty.length

    listProperty: [ QtObject{}, QtObject {} ]
}

