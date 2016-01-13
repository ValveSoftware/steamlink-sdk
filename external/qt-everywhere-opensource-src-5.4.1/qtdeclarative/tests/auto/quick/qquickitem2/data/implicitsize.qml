import QtQuick 2.0

Item {
    implicitWidth: 200
    implicitHeight: 100

    width: 80
    height: 60

    function resetSize() {
        width = undefined
        height = undefined
    }

    function changeImplicit() {
        implicitWidth = 150
        implicitHeight = 80
    }

    function increaseImplicit() {
        implicitWidth = 200
        implicitHeight = 100
    }

    function assignImplicitBinding() {
        width = Qt.binding(function() { return implicitWidth < 175 ? implicitWidth : 175 })
        height = Qt.binding(function() { return implicitHeight < 90 ? implicitHeight : 90 })
    }

    function assignUndefinedBinding() {
        width = Qt.binding(function() { return implicitWidth < 175 ? undefined : 175 })
        height = Qt.binding(function() { return implicitHeight < 90 ? undefined : 90 })
    }
}
