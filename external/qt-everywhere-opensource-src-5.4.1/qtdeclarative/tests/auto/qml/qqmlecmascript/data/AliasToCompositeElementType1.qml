import QtQuick 2.0

QtObject {
    property alias group: obj
    property variant foo: AliasToCompositeElementType2 { id: obj }
}
