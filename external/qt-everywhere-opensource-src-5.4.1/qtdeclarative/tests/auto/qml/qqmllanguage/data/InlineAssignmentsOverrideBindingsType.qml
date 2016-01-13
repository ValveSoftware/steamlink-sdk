import QtQuick 2.0

QtObject {
    property InlineAssignmentsOverrideBindingsType2 nested: InlineAssignmentsOverrideBindingsType2 {
        value: 19 * 33
    }
}
