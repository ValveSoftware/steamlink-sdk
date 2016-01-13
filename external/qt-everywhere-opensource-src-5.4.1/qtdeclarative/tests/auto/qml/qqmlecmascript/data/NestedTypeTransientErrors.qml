import QtQuick 2.0

QtObject {
    property int b: obj.prop.a

    property variant prop;
    prop: QtObject {
        property int a: 10
    }
}

