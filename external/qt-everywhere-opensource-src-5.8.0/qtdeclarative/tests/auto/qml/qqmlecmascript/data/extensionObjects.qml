import Qt.test 1.0
import QtQuick 2.0

MyExtendedObject
{
    baseProperty: baseExtendedProperty
    baseExtendedProperty: 13

    coreProperty: extendedProperty
    extendedProperty: 9

    property QtObject nested: MyExtendedObject {
        baseProperty: baseExtendedProperty
        baseExtendedProperty: 13

        coreProperty: extendedProperty
        extendedProperty: 9
    }
}
