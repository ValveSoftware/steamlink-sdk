import QtQuick 2.0

Item {
    property bool success: false

    property date localDate
    property date utcDate

    Component.onCompleted: {
        // Test date: 2012-06-01T02:15:30+10:00 (AEST timezone)
        localDate = new Date(2012, 6-1, 1, 2, 15, 30)
        utcDate = new Date(Date.UTC(2012, 6-1, 1, 2, 15, 30))

        if (localDate.getTimezoneOffset() != -600) return

        if (localDate.toLocaleString() != getLocalizedForm('2012-06-01T02:15:30')) return
        if (localDate.toISOString() != "2012-05-31T16:15:30.000Z") return

        if (utcDate.toISOString() != "2012-06-01T02:15:30.000Z") return
        if (utcDate.toLocaleString() != getLocalizedForm('2012-06-01T12:15:30')) return

        success = true
    }

    function check() {
        success = false

        // We have changed to IST time zone - inform JS:
        Date.timeZoneUpdated()

        if (localDate.getTimezoneOffset() != -330) return

        if (localDate.toLocaleString() != getLocalizedForm('2012-06-01T02:15:30')) return
        if (localDate.toISOString() != "2012-05-31T20:45:30.000Z") return

        if (utcDate.toISOString() != "2012-06-01T06:45:30.000Z") return
        if (utcDate.toLocaleString() != getLocalizedForm("2012-06-01T12:15:30")) return

        // Create new dates in this timezone
        localDate = new Date(2012, 6-1, 1, 2, 15, 30)
        utcDate = new Date(Date.UTC(2012, 6-1, 1, 2, 15, 30))

        if (localDate.toLocaleString() != getLocalizedForm("2012-06-01T02:15:30")) return
        if (localDate.toISOString() != "2012-05-31T20:45:30.000Z") return

        if (utcDate.toISOString() != "2012-06-01T02:15:30.000Z") return
        if (utcDate.toLocaleString() != getLocalizedForm("2012-06-01T07:45:30")) return

        success = true
    }

    function resetTimeZone() {
        Date.timeZoneUpdated()
    }
}
