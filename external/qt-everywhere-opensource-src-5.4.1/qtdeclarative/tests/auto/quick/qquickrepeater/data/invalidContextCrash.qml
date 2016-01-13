import QtQuick 2.0
Item {
    Repeater {
        model: badModel
        delegate: Item {}
    }
}
