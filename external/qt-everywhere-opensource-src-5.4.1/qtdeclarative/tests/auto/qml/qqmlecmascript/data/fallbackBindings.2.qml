import QtQuick 2.0

Item {
    property bool success: false

    BaseComponent {
        id: foo
        property Text baz: Text { width: 200 }
    }

    // With contextual lookup, 'bar' is resolved in the BaseComponent context,
    // and refers to the 'baz' defined there; in this context, however, 'baz'
    // resolves to the override defined in 'foo'
    Component.onCompleted: success = (foo.bar == 100) && (foo.baz.width == 200)
}
