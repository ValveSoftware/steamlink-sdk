import QtQuick 2.0

QtObject {
    property int foo1: 100
    property int foo2: 100
    property int foo3: { return 100; }
    property int foo4: { return 100; }

    property string bar1: 'Hello'
    property string bar2: 'Hello'
    property string bar3: { return 'Hello'; }
    property string bar4: { return 'Hello'; }
}
