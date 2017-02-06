import QtQuick 2.0

QtObject {
    property var locale: Qt.locale()

    function setLocale(l) {
        locale = Qt.locale(l)
    }

    function toLocaleString(n,fmt,prec) {
        if (prec < 0)
            return n.toLocaleString(locale, fmt);
        else
            return n.toLocaleString(locale, fmt, prec);
    }

    function toLocaleCurrencyString(n,symbol) {
        if (symbol.length == 0)
            return n.toLocaleCurrencyString(locale);
        else
            return n.toLocaleCurrencyString(locale, symbol);
    }

    function fromLocaleString(n) {
        return Number.fromLocaleString(locale, n)
    }

    property var const1: 1234.56.toLocaleString(locale);
    property var const2: 1234..toLocaleString(locale);
}
