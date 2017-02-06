import QtQuick 2.0

Item {
    property string amText: Qt.inputMethod.locale.amText
    property string decimalPoint: Qt.inputMethod.locale.decimalPoint
    property string exponential: Qt.inputMethod.locale.exponential
    property int firstDayOfWeek: Qt.inputMethod.locale.firstDayOfWeek
    property string groupSeparator: Qt.inputMethod.locale.groupSeparator
    property int measurementSystem: Qt.inputMethod.locale.measurementSystem
    property string name: Qt.inputMethod.locale.name
    property string nativeCountryName: Qt.inputMethod.locale.nativeCountryName
    property string nativeLanguageName: Qt.inputMethod.locale.nativeLanguageName
    property string negativeSign: Qt.inputMethod.locale.negativeSign
    property string percent: Qt.inputMethod.locale.percent
    property string pmText: Qt.inputMethod.locale.pmText
    property string positiveSign: Qt.inputMethod.locale.positiveSign
    property int textDirection: Qt.inputMethod.locale.textDirection
    property var uiLanguages: Qt.inputMethod.locale.uiLanguages
    property var weekDays: Qt.inputMethod.locale.weekDays
    property string zeroDigit: Qt.inputMethod.locale.zeroDigit
}

