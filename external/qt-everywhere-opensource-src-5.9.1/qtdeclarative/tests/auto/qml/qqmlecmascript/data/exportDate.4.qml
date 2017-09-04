import Qt.test 1.0
import QtQuick 2.0

MyTypeObject {
    boolProperty: false

    Component.onCompleted: {
        var dt = datetimeExporter.getDateTime()
        var offset = datetimeExporter.getDateTimeOffset()
        var date = datetimeExporter.getDate()
        var timespec = datetimeExporter.getTimeSpec()

        // The test date is 2009-5-12 23:59:59 (UTC)
        var compare = new Date(Date.UTC(2009, 5-1, 12, 23, 59, 59))
        var compareOffset = 0

        // Adjust for timezone to extract correct partial values
        var dtAdjusted = new Date(dt.getUTCFullYear(),
                                  dt.getUTCMonth(),
                                  dt.getUTCDate(),
                                  dt.getUTCHours(),
                                  dt.getUTCMinutes(),
                                  dt.getUTCSeconds(),
                                  dt.getUTCMilliseconds())

        boolProperty = (dt.getTime() == compare.getTime()) &&
                       (offset == compareOffset) &&
                       (timespec == 'UTC') &&
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
