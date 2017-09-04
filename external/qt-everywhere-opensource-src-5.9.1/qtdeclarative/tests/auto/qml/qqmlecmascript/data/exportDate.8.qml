import Qt.test 1.0
import QtQuick 2.0

MyTypeObject {
    boolProperty: false

    Component.onCompleted: {
        var dt = datetimeExporter.getDateTime()
        var offset = datetimeExporter.getDateTimeOffset()
        var date = datetimeExporter.getDate()
        var timespec = datetimeExporter.getTimeSpec()

        // The test date is 2009-5-12 23:59:59 (UTC-11:30)
        var compare = new Date('2009-05-12T23:59:59-11:30')

        // Adjust for timezone to extract correct partial values
        var dtUtc = new Date(dt.getTime() + (offset * 60000))
        var dtAdjusted = new Date(dtUtc.getUTCFullYear(),
                                  dtUtc.getUTCMonth(),
                                  dtUtc.getUTCDate(),
                                  dtUtc.getUTCHours(),
                                  dtUtc.getUTCMinutes(),
                                  dtUtc.getUTCSeconds(),
                                  dtUtc.getUTCMilliseconds())

        boolProperty = (dt.getTime() == compare.getTime()) &&
                       (offset == -((11 * 60) + 30)) &&
                       (timespec == '-11:30') &&
                       (dtAdjusted.getFullYear() == 2009) &&
                       (dtAdjusted.getMonth() == 5-1) &&
                       (dtAdjusted.getDate() == 12) &&
                       (dtAdjusted.getHours() == 23) &&
                       (dtAdjusted.getMinutes() == 59) &&
                       (dtAdjusted.getSeconds() == 59) &&
                       (date.getFullYear() == 2009) &&
                       (date.getMonth() == 5-1) &&
                       (date.getDate() == 12)
    }
}
