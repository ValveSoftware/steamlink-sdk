import QtQuick 1.0

Item {
    id: screen

    Connections { target: screen; onNotFooBar1: {} ignoreUnknownSignals: true }
    Connections { objectName: "connections"; onNotFooBar2: {} ignoreUnknownSignals: true }
}
