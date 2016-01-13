import QtQuick 2.0

OverrideSignalComponent {
    signal objectNameChanged // manual signal override of builtin signal, invalid.

    // due to an unrelated bug (QTBUG-26818), a certain
    // number of properties are needed to exist before the
    // crash condition is hit, currently.
    property int a
    property int b
    property int c
    property int d
    property int e
    property int f
    property int g
    property int h
    property int i
    property int j
    property int k
    property int l
    property int m
    property int n
}
