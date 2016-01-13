import QtQuick 2.0

QtObject {
    property QtObject o;
    property alias a: object.a
    o: NestedAlias { id: object }
}

