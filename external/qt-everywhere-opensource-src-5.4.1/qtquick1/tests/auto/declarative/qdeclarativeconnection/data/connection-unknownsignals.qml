import QtQuick 1.0

Item {
    id: screen

    Connections { objectName: "connections"; target: screen; onFooBar: {} }
}
