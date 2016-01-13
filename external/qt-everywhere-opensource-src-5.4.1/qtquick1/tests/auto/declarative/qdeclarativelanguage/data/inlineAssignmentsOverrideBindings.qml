import QtQuick 1.0

InlineAssignmentsOverrideBindingsType {
    property int test: nested.value
    nested.value: 11
}
