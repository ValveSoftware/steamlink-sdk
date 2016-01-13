import QtQuick 2.0

QQmlDataDestroyedComponent2Base {
    id: derived
    color: "black" // will trigger change signal during beginCreate.
    x: 2 // flip ownership of base
}
