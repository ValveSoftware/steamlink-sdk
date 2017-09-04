import QtQuick 2.0

OverrideSignalComponent {
    property bool success: (testChangedCount == 1)
    property int testChangedCount: 0
    signal testChanged // override change signal from super

    Component.onCompleted: {
        test = 20;
        testChanged();
    }

    onTestChanged: testChangedCount = 1; // override the signal, change handler won't be called.

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
