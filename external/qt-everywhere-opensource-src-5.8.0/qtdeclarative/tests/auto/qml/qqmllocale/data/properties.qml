import QtQuick 2.0

QtObject {
    property var locale: Qt.locale()

    function setLocale(l) {
        locale = Qt.locale(l);
    }

    property var name: locale.name
    property var amText: locale.amText
    property var pmText: locale.pmText
    property var nativeLanguageName: locale.nativeLanguageName
    property var nativeCountryName: locale.nativeCountryName
    property var decimalPoint: locale.decimalPoint
    property var groupSeparator: locale.groupSeparator
    property var percent: locale.percent
    property var zeroDigit: locale.zeroDigit
    property var negativeSign: locale.negativeSign
    property var positiveSign: locale.positiveSign
    property var exponential: locale.exponential
    property var firstDayOfWeek: locale.firstDayOfWeek
    property var measurementSystem: locale.measurementSystem
    property var textDirection: locale.textDirection
    property var weekDays: locale.weekDays
    property var uiLanguages: locale.uiLanguages
}
