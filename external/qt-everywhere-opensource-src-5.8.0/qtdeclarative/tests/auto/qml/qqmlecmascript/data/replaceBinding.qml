import QtQuick 2.0

OuterObject {
    property bool success: false

    inner.foo1: 200
    inner.foo2: { return 200; }
    inner.foo3: 200
    inner.foo4: { return 200; }

    inner.bar1: 'Goodbye'
    inner.bar2: { return 'Goodbye' }
    inner.bar3: 'Goodbye'
    inner.bar4: { return 'Goodbye' }

    Component.onCompleted: {
        success = (inner.foo1 == 200 &&
                   inner.foo2 == 200 &&
                   inner.foo3 == 200 &&
                   inner.foo4 == 200 &&
                   inner.bar1 == 'Goodbye' &&
                   inner.bar2 == 'Goodbye' &&
                   inner.bar3 == 'Goodbye' &&
                   inner.bar4 == 'Goodbye');
    }
}
