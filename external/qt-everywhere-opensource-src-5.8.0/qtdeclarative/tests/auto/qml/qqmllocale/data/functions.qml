import QtQuick 2.0

QtObject {
    property var locale: Qt.locale()

    function setLocale(l) {
        locale = Qt.locale(l)
    }

    function currencySymbol(type) {
        if (type < 0)
            return locale.currencySymbol()
        else
            return locale.currencySymbol(type)
    }

    function monthName(month,type) {
        if (type < 0)
            return locale.monthName(month)
        else
            return locale.monthName(month, type)
    }

    function standaloneMonthName(month,type) {
        if (type < 0)
            return locale.standaloneMonthName(month)
        else
            return locale.standaloneMonthName(month, type)
    }

    function dayName(month,type) {
        if (type < 0)
            return locale.dayName(month)
        else
            return locale.dayName(month, type)
    }

    function standaloneDayName(month,type) {
        if (type < 0)
            return locale.standaloneDayName(month)
        else
            return locale.standaloneDayName(month, type)
    }

    function dateTimeFormat(fmt) {
        if (fmt < 0)
            return locale.dateTimeFormat()
        else
            return locale.dateTimeFormat(fmt)
    }

    function dateFormat(fmt) {
        if (fmt < 0)
            return locale.dateFormat()
        else
            return locale.dateFormat(fmt)
    }

    function timeFormat(fmt) {
        if (fmt < 0)
            return locale.timeFormat()
        else
            return locale.timeFormat(fmt)
    }
}
