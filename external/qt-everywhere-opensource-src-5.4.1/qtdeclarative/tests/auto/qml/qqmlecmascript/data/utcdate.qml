import QtQuick 2.0
import Qt.test 1.0

Item {
    MyDateClass {
        id: mdc
    }

    function check_utc(utcstr) {
        var datetimeutc = utcstr.split('T')
        var dateutc = datetimeutc[0].split('-')
        var timeutc = datetimeutc[1].split(':')
        var utcDate = new Date(0)
        utcDate.setUTCFullYear(Number(dateutc[0]))
        utcDate.setUTCMonth(Number(dateutc[1])-1)
        utcDate.setUTCDate(Number(dateutc[2]))
        utcDate.setUTCHours(Number(timeutc[0]))
        utcDate.setUTCMinutes(Number(timeutc[1]))
        utcDate.setUTCSeconds(Number(timeutc[2]))
        if (utcDate.getUTCFullYear() != Number(dateutc[0]))
            return false;
        if (utcDate.getUTCMonth() != Number(dateutc[1])-1)
            return false;
        if (utcDate.getUTCDate() != Number(dateutc[2]))
            return false;
        if (utcDate.getUTCHours() != Number(timeutc[0]))
            return false;
        if (utcDate.getUTCMinutes() != Number(timeutc[1]))
            return false;
        if (utcDate.getUTCSeconds() != Number(timeutc[2]))
            return false;
        return true;
    }
}
