import QtQuick 2.0

QtObject {
    property var locale: Qt.locale()

    function setLocale(l) {
        locale = Qt.locale(l)
    }

    function toLocaleString(fmt) {
        var d = new Date(2011, 9, 7, 18, 53, 48, 345);
        if (fmt < 0)
            return d.toLocaleString(locale);
        else
            return d.toLocaleString(locale, fmt);
    }

    function toLocaleDateString(fmt) {
        var d = new Date(2011, 9, 7, 18, 53, 48, 345);
        if (fmt < 0)
            return d.toLocaleDateString(locale);
        else
            return d.toLocaleDateString(locale, fmt);
    }

    function toLocaleTimeString(fmt) {
        var d = new Date(2011, 9, 7, 18, 53, 48, 345);
        if (fmt < 0)
            return d.toLocaleTimeString(locale);
        else
            return d.toLocaleTimeString(locale, fmt);
    }

    function fromLocaleString(d,fmt) {
        return Date.fromLocaleString(locale, d, fmt)
    }

    function fromLocaleDateString(d,fmt) {
        return Date.fromLocaleDateString(locale, d, fmt)
    }

    function fromLocaleTimeString(d,fmt) {
        return Date.fromLocaleTimeString(locale, d, fmt)
    }
}
