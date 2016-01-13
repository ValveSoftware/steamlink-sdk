import QtQuick 1.0

QtObject {
    property date dateFromString: "2008-12-24"
    property variant jsdate: new Date(2008,11,24,14,15,38,200) // months are 0-based

    function formatDate(prop) {
        var v = eval(prop)
        return [
            Qt.formatDate(v),
            Qt.formatDate(v, Qt.DefaultLocaleLongDate),
            Qt.formatDate(v, "ddd MMMM d yy")
        ]
    }

    function formatTime(prop) {
        var v = eval(prop)
        return [
            Qt.formatTime(v),
            Qt.formatTime(v, Qt.DefaultLocaleLongDate),
            Qt.formatTime(v, "H:m:s a"),
            Qt.formatTime(v, "hh:mm:ss.zzz")
        ]
    }

    function formatDateTime(prop) {
        var v = eval(prop)
        return [
            Qt.formatDateTime(v),
            Qt.formatDateTime(v, Qt.DefaultLocaleLongDate),
            Qt.formatDateTime(v, "M/d/yy H:m:s a")
        ]
    }

    // Error cases
    property string err_date1: Qt.formatDate()
    property string err_date2: Qt.formatDate(new Date, new Object)

    property string err_time1: Qt.formatTime()
    property string err_time2: Qt.formatTime(new Date, new Object)

    property string err_dateTime1: Qt.formatDateTime()
    property string err_dateTime2: Qt.formatDateTime(new Date, new Object)
}
