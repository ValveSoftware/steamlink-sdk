import QtQuick 2.0

Item
{
    property int someInt: 4
    property var variantArray: [1, 2]
    property int arrayLength: 0

    onSomeIntChanged:
    {
        arrayLength = variantArray.length
    }
}
