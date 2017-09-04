import QtQuick 2.0

QtObject {
    id: root

    property bool test: bound.value == 1923

    property ElementAssignType element: ElementAssignType { value: 1923 }
    property ElementAssignType bound: root.element
}
