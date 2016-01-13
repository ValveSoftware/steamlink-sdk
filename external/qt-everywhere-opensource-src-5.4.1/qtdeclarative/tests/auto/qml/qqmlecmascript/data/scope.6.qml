import QtQuick 2.0

Item {
    id: me
    property bool test: nested.runtest(me);

    Scope6Nested {
        id: nested
    }
}
