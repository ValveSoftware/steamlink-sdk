import QtQuick 2.0

Item {
    id: root
    property real a: 50
    ParallelAnimation {
        id: anim
        ScriptAction { script: root.a += 5; }
        ScriptAction { script: root.a += 15; }
    }
}
