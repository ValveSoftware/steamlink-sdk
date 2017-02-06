import QtQuick 2.5
import QtQuick.Controls 2.1

RangeSlider {
    from: 0
    to: 100
    first.value: 25
    second.value: 75
    stepSize: 1
    orientation: Qt.Horizontal
}
