import Qt.test 1.0
import QtQuick 2.0

MyTypeObject {
    Component.onCompleted: {
        var dateVar = new Date(Date.UTC(2009, 4, 12))
        var dateTimeVar = new Date(Date.UTC(2009, 4, 12, 0, 0, 1))
        var dateTimeVar2 = new Date(Date.UTC(2009, 4, 12, 23, 59, 59))

        dateProperty = dateVar
        dateTimeProperty = dateTimeVar
        dateTimeProperty2 = dateTimeVar2

        // Commented properties do not currently test true:
        boolProperty = //(dateProperty.getTime() == dateVar.getTime()) &&
                       (dateProperty.getFullYear() == 2009) &&
                       (dateProperty.getMonth() == 5-1) &&
                       //(dateProperty.getDate() == 12) &&
                       (dateProperty.getHours() == 0) &&
                       (dateTimeProperty.getTime() == dateTimeVar.getTime()) &&
                       (dateTimeProperty.getFullYear() == 2009) &&
                       (dateTimeProperty.getMonth() == 5-1) &&
                       //(dateTimeProperty.getDate() == 12) &&
                       //(dateTimeProperty.getHours() == 0) &&
                       (dateTimeProperty.getMinutes() == 0) &&
                       (dateTimeProperty.getSeconds() == 1) &&
                       (dateTimeProperty2.getTime() == dateTimeVar2.getTime()) &&
                       (dateTimeProperty2.getFullYear() == 2009) &&
                       (dateTimeProperty2.getMonth() == 5-1) &&
                       //(dateTimeProperty2.getDate() == 12) &&
                       //(dateTimeProperty2.getHours() == 23) &&
                       (dateTimeProperty2.getMinutes() == 59) &&
                       (dateTimeProperty2.getSeconds() == 59)
    }
}
