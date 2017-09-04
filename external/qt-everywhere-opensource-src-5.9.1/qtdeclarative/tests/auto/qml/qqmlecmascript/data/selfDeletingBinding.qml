import Qt.test 1.0

MyQmlContainer {
    property bool triggerDelete: false

    children: [
        MyQmlObject {
            // Will trigger deletion during binding evaluation
            stringProperty: {deleteMe(), "Hello"}
        },

        MyQmlObject {
            // Will trigger deletion during binding evaluation, but after component creation
            stringProperty: if (triggerDelete) { deleteMe(), "Hello" } else { "World" }
        }

    ]
}
