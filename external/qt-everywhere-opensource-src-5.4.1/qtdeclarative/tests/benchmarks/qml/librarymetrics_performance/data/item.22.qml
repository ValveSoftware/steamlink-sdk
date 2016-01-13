import QtQuick 2.0

Item {
    property int b: 100
    property int c: 15
    property int a: 15
    onAChanged: {
        if (b >= 100) b = 12;
        else c += a;
    }
}
