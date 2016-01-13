import QtQuick 2.0

// This test uses a multi-line string which has \r-terminated
// string fragments.  The expression rewriter deliberately doesn't
// handle \r-terminated string fragments (see QTBUG-24064) and thus
// this test ensures that we don't crash when we encounter a
// non-compilable binding such as this one.

Item {
    id: root

    Component {
        id: comp
        Text {
            property var value: ","
            text: 'multi            line ' + value + 'str            ings'
        }
    }

    Component.onCompleted: comp.createObject(root, { "value": undefined })
}
