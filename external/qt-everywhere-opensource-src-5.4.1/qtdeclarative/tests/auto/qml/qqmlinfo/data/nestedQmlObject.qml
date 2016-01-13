import QtQuick 2.0

QtObject {
    property variant nested
    nested: NestedObject { }
    property variant nested2: nested.nested
}

