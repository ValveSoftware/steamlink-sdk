import QtQuick 2.0
import QtQml.Models 2.1

ListView {
    width: 360
    height: 360
    model: ObjectModel {
        Rectangle {
            width: 20
            height: 20
            color: "red"
        }
        Rectangle {
            width: 20
            height: 20
            color: "green"
        }
        Rectangle {
            width: 20
            height: 20
            color: "blue"
        }
    }
}
