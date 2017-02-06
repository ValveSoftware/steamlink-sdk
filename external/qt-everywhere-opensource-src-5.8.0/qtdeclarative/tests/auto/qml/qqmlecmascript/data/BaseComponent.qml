import QtQuick 2.0

Item {
    property Item baz: Item { width: 100 }
    property string bar: baz.width
}
