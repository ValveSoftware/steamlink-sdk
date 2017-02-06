import Qt.test 1.0

MyQmlContainer {
    property bool triggerDelete: false

    children: [
        MyQmlObject {
            // Will trigger deletion on binding assignment
            deleteOnSet: Math.max(0, 1)
        },

        MyQmlObject {
            // Will trigger deletion on binding assignment, but after component creation
            deleteOnSet: if (triggerDelete) 1; else 0;
        }
    ]
}
