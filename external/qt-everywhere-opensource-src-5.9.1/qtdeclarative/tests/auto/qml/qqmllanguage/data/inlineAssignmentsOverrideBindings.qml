import QtQuick 2.0

InlineAssignmentsOverrideBindingsType {
    property int test: nested.value
    nested.value: 11
}
