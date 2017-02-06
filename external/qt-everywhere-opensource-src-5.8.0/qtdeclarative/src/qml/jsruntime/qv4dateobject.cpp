/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include "qv4dateobject_p.h"
#include "qv4objectproto_p.h"
#include "qv4scopedvalue_p.h"
#include "qv4runtime_p.h"
#include "qv4string_p.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QStringList>

#include <time.h>

#include <private/qqmljsengine_p.h>

#include <wtf/MathExtras.h>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  ifndef Q_OS_VXWORKS
#    include <sys/time.h>
#  else
#    include "qplatformdefs.h"
#  endif
#  include <unistd.h> // for _POSIX_THREAD_SAFE_FUNCTIONS
#endif

using namespace QV4;

static const double HoursPerDay = 24.0;
static const double MinutesPerHour = 60.0;
static const double SecondsPerMinute = 60.0;
static const double msPerSecond = 1000.0;
static const double msPerMinute = 60000.0;
static const double msPerHour = 3600000.0;
static const double msPerDay = 86400000.0;

static double LocalTZA = 0.0; // initialized at startup

static inline double TimeWithinDay(double t)
{
    double r = ::fmod(t, msPerDay);
    return (r >= 0) ? r : r + msPerDay;
}

static inline int HourFromTime(double t)
{
    int r = int(::fmod(::floor(t / msPerHour), HoursPerDay));
    return (r >= 0) ? r : r + int(HoursPerDay);
}

static inline int MinFromTime(double t)
{
    int r = int(::fmod(::floor(t / msPerMinute), MinutesPerHour));
    return (r >= 0) ? r : r + int(MinutesPerHour);
}

static inline int SecFromTime(double t)
{
    int r = int(::fmod(::floor(t / msPerSecond), SecondsPerMinute));
    return (r >= 0) ? r : r + int(SecondsPerMinute);
}

static inline int msFromTime(double t)
{
    int r = int(::fmod(t, msPerSecond));
    return (r >= 0) ? r : r + int(msPerSecond);
}

static inline double Day(double t)
{
    return ::floor(t / msPerDay);
}

static inline double DaysInYear(double y)
{
    if (::fmod(y, 4))
        return 365;

    else if (::fmod(y, 100))
        return 366;

    else if (::fmod(y, 400))
        return 365;

    return 366;
}

static inline double DayFromYear(double y)
{
    return 365 * (y - 1970)
        + ::floor((y - 1969) / 4)
        - ::floor((y - 1901) / 100)
        + ::floor((y - 1601) / 400);
}

static inline double TimeFromYear(double y)
{
    return msPerDay * DayFromYear(y);
}

static inline double YearFromTime(double t)
{
    int y = 1970;
    y += (int) ::floor(t / (msPerDay * 365.2425));

    double t2 = TimeFromYear(y);
    return (t2 > t) ? y - 1 : ((t2 + msPerDay * DaysInYear(y)) <= t) ? y + 1 : y;
}

static inline bool InLeapYear(double t)
{
    double x = DaysInYear(YearFromTime(t));
    if (x == 365)
        return 0;

    Q_ASSERT(x == 366);
    return 1;
}

static inline double DayWithinYear(double t)
{
    return Day(t) - DayFromYear(YearFromTime(t));
}

static inline double MonthFromTime(double t)
{
    double d = DayWithinYear(t);
    double l = InLeapYear(t);

    if (d < 31.0)
        return 0;

    else if (d < 59.0 + l)
        return 1;

    else if (d < 90.0 + l)
        return 2;

    else if (d < 120.0 + l)
        return 3;

    else if (d < 151.0 + l)
        return 4;

    else if (d < 181.0 + l)
        return 5;

    else if (d < 212.0 + l)
        return 6;

    else if (d < 243.0 + l)
        return 7;

    else if (d < 273.0 + l)
        return 8;

    else if (d < 304.0 + l)
        return 9;

    else if (d < 334.0 + l)
        return 10;

    else if (d < 365.0 + l)
        return 11;

    return qt_qnan(); // ### assert?
}

static inline double DateFromTime(double t)
{
    int m = (int) Primitive::toInteger(MonthFromTime(t));
    double d = DayWithinYear(t);
    double l = InLeapYear(t);

    switch (m) {
    case 0: return d + 1.0;
    case 1: return d - 30.0;
    case 2: return d - 58.0 - l;
    case 3: return d - 89.0 - l;
    case 4: return d - 119.0 - l;
    case 5: return d - 150.0 - l;
    case 6: return d - 180.0 - l;
    case 7: return d - 211.0 - l;
    case 8: return d - 242.0 - l;
    case 9: return d - 272.0 - l;
    case 10: return d - 303.0 - l;
    case 11: return d - 333.0 - l;
    }

    return qt_qnan(); // ### assert
}

static inline double WeekDay(double t)
{
    double r = ::fmod (Day(t) + 4.0, 7.0);
    return (r >= 0) ? r : r + 7.0;
}


static inline double MakeTime(double hour, double min, double sec, double ms)
{
    return ((hour * MinutesPerHour + min) * SecondsPerMinute + sec) * msPerSecond + ms;
}

static inline double DayFromMonth(double month, double leap)
{
    switch ((int) month) {
    case 0: return 0;
    case 1: return 31.0;
    case 2: return 59.0 + leap;
    case 3: return 90.0 + leap;
    case 4: return 120.0 + leap;
    case 5: return 151.0 + leap;
    case 6: return 181.0 + leap;
    case 7: return 212.0 + leap;
    case 8: return 243.0 + leap;
    case 9: return 273.0 + leap;
    case 10: return 304.0 + leap;
    case 11: return 334.0 + leap;
    }

    return qt_qnan(); // ### assert?
}

static double MakeDay(double year, double month, double day)
{
    year += ::floor(month / 12.0);

    month = ::fmod(month, 12.0);
    if (month < 0)
        month += 12.0;

    double d = DayFromYear(year);
    bool leap = InLeapYear(d*msPerDay);

    d += DayFromMonth(month, leap);
    d += day - 1;

    return d;
}

static inline double MakeDate(double day, double time)
{
    return day * msPerDay + time;
}

static inline double DaylightSavingTA(double t)
{
    struct tm tmtm;
#if defined(_MSC_VER) && _MSC_VER >= 1400
    __time64_t  tt = (__time64_t)(t / msPerSecond);
    // _localtime_64_s returns non-zero on failure
    if (_localtime64_s(&tmtm, &tt) != 0)
#elif !defined(QT_NO_THREAD) && defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    long int tt = (long int)(t / msPerSecond);
    if (!localtime_r((const time_t*) &tt, &tmtm))
#else
    // Returns shared static data which may be overwritten at any time
    // (for MinGW/Windows localtime is luckily thread safe)
    long int tt = (long int)(t / msPerSecond);
    if (struct tm *tmtm_p = localtime((const time_t*) &tt))
        tmtm = *tmtm_p;
    else
#endif
        return 0;
    return (tmtm.tm_isdst > 0) ? msPerHour : 0;
}

static inline double LocalTime(double t)
{
    return t + LocalTZA + DaylightSavingTA(t);
}

static inline double UTC(double t)
{
    return t - LocalTZA - DaylightSavingTA(t - LocalTZA);
}

static inline double currentTime()
{
#ifndef Q_OS_WIN
    struct timeval tv;

    gettimeofday(&tv, 0);
    return ::floor(tv.tv_sec * msPerSecond + (tv.tv_usec / 1000.0));
#else
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    LARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    return double(li.QuadPart - Q_INT64_C(116444736000000000)) / 10000.0;
#endif
}

static inline double TimeClip(double t)
{
    if (! qt_is_finite(t) || fabs(t) > 8.64e15)
        return qt_qnan();
    return Primitive::toInteger(t);
}

static inline double ParseString(const QString &s)
{
    // first try the format defined in 15.9.1.15, only if that fails fall back to
    // QDateTime for parsing

    // the define string format is YYYY-MM-DDTHH:mm:ss.sssZ
    // It can be date or time only, and the second and later components
    // of both fields are optional
    // and extended syntax for negative and large positive years exists: +/-YYYYYY

    enum Format {
        Year,
        Month,
        Day,
        Hour,
        Minute,
        Second,
        MilliSecond,
        TimezoneHour,
        TimezoneMinute,
        Done
    };

    const QChar *ch = s.constData();
    const QChar *end = ch + s.length();

    uint format = Year;
    int current = 0;
    int currentSize = 0;
    bool extendedYear = false;

    int yearSign = 1;
    int year = 0;
    int month = 0;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int msec = 0;
    int offsetSign = 1;
    int offset = 0;

    bool error = false;
    if (*ch == '+' || *ch == '-') {
        extendedYear = true;
        if (*ch == '-')
            yearSign = -1;
        ++ch;
    }
    while (ch <= end) {
        if (*ch >= '0' && *ch <= '9') {
            current *= 10;
            current += ch->unicode() - '0';
            ++currentSize;
        } else { // other char, delimits field
            switch (format) {
            case Year:
                year = current;
                if (extendedYear)
                    error = (currentSize != 6);
                else
                    error = (currentSize != 4);
                break;
            case Month:
                month = current - 1;
                error = (currentSize != 2) || month > 11;
                break;
            case Day:
                day = current;
                error = (currentSize != 2) || day > 31;
                break;
            case Hour:
                hour = current;
                error = (currentSize != 2) || hour > 24;
                break;
            case Minute:
                minute = current;
                error = (currentSize != 2) || minute > 60;
                break;
            case Second:
                second = current;
                error = (currentSize != 2) || second > 60;
                break;
            case MilliSecond:
                msec = current;
                error = (currentSize != 3);
                break;
            case TimezoneHour:
                offset = current*60;
                error = (currentSize != 2) || offset > 23*60;
                break;
            case TimezoneMinute:
                offset += current;
                error = (currentSize != 2) || current >= 60;
                break;
            }
            if (*ch == 'T') {
                if (format >= Hour)
                    error = true;
                format = Hour;
            } else if (*ch == '-') {
                if (format < Day)
                    ++format;
                else if (format < Minute)
                    error = true;
                else if (format >= TimezoneHour)
                    error = true;
                else {
                    offsetSign = -1;
                    format = TimezoneHour;
                }
            } else if (*ch == ':') {
                if (format != Hour && format != Minute && format != TimezoneHour)
                    error = true;
                ++format;
            } else if (*ch == '.') {
                if (format != Second)
                    error = true;
                ++format;
            } else if (*ch == '+') {
                if (format < Minute || format >= TimezoneHour)
                    error = true;
                format = TimezoneHour;
            } else if (*ch == 'Z' || ch->unicode() == 0) {
                format = Done;
            }
            current = 0;
            currentSize = 0;
        }
        if (error || format == Done)
            break;
        ++ch;
    }

    if (!error) {
        double t = MakeDate(MakeDay(year * yearSign, month, day), MakeTime(hour, minute, second, msec));
        t -= offset * offsetSign * 60 * 1000;
        return t;
    }

    QDateTime dt = QDateTime::fromString(s, Qt::TextDate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::RFC2822Date);
    if (!dt.isValid()) {
        QStringList formats;
        formats << QStringLiteral("M/d/yyyy")
                << QStringLiteral("M/d/yyyy hh:mm")
                << QStringLiteral("M/d/yyyy hh:mm A")

                << QStringLiteral("M/d/yyyy, hh:mm")
                << QStringLiteral("M/d/yyyy, hh:mm A")

                << QStringLiteral("MMM d yyyy")
                << QStringLiteral("MMM d yyyy hh:mm")
                << QStringLiteral("MMM d yyyy hh:mm:ss")
                << QStringLiteral("MMM d yyyy, hh:mm")
                << QStringLiteral("MMM d yyyy, hh:mm:ss")

                << QStringLiteral("MMMM d yyyy")
                << QStringLiteral("MMMM d yyyy hh:mm")
                << QStringLiteral("MMMM d yyyy hh:mm:ss")
                << QStringLiteral("MMMM d yyyy, hh:mm")
                << QStringLiteral("MMMM d yyyy, hh:mm:ss")

                << QStringLiteral("MMM d, yyyy")
                << QStringLiteral("MMM d, yyyy hh:mm")
                << QStringLiteral("MMM d, yyyy hh:mm:ss")

                << QStringLiteral("MMMM d, yyyy")
                << QStringLiteral("MMMM d, yyyy hh:mm")
                << QStringLiteral("MMMM d, yyyy hh:mm:ss")

                << QStringLiteral("d MMM yyyy")
                << QStringLiteral("d MMM yyyy hh:mm")
                << QStringLiteral("d MMM yyyy hh:mm:ss")
                << QStringLiteral("d MMM yyyy, hh:mm")
                << QStringLiteral("d MMM yyyy, hh:mm:ss")

                << QStringLiteral("d MMMM yyyy")
                << QStringLiteral("d MMMM yyyy hh:mm")
                << QStringLiteral("d MMMM yyyy hh:mm:ss")
                << QStringLiteral("d MMMM yyyy, hh:mm")
                << QStringLiteral("d MMMM yyyy, hh:mm:ss")

                << QStringLiteral("d MMM, yyyy")
                << QStringLiteral("d MMM, yyyy hh:mm")
                << QStringLiteral("d MMM, yyyy hh:mm:ss")

                << QStringLiteral("d MMMM, yyyy")
                << QStringLiteral("d MMMM, yyyy hh:mm")
                << QStringLiteral("d MMMM, yyyy hh:mm:ss");

        for (int i = 0; i < formats.size(); ++i) {
            dt = QDateTime::fromString(s, formats.at(i));
            if (dt.isValid())
                break;
        }
    }
    if (!dt.isValid())
        return qt_qnan();
    return dt.toMSecsSinceEpoch();
}

/*!
  \internal

  Converts the ECMA Date value \tt (in UTC form) to QDateTime
  according to \a spec.
*/
static inline QDateTime ToDateTime(double t, Qt::TimeSpec spec)
{
    if (std::isnan(t))
        return QDateTime();
    return QDateTime::fromMSecsSinceEpoch(t, spec);
}

static inline QString ToString(double t)
{
    if (std::isnan(t))
        return QStringLiteral("Invalid Date");
    QString str = ToDateTime(t, Qt::LocalTime).toString() + QLatin1String(" GMT");
    double tzoffset = LocalTZA + DaylightSavingTA(t);
    if (tzoffset) {
        int hours = static_cast<int>(::fabs(tzoffset) / 1000 / 60 / 60);
        int mins = int(::fabs(tzoffset) / 1000 / 60) % 60;
        str.append(QLatin1Char((tzoffset > 0) ?  '+' : '-'));
        if (hours < 10)
            str.append(QLatin1Char('0'));
        str.append(QString::number(hours));
        if (mins < 10)
            str.append(QLatin1Char('0'));
        str.append(QString::number(mins));
    }
    return str;
}

static inline QString ToUTCString(double t)
{
    if (std::isnan(t))
        return QStringLiteral("Invalid Date");
    return ToDateTime(t, Qt::UTC).toString();
}

static inline QString ToDateString(double t)
{
    return ToDateTime(t, Qt::LocalTime).date().toString();
}

static inline QString ToTimeString(double t)
{
    return ToDateTime(t, Qt::LocalTime).time().toString();
}

static inline QString ToLocaleString(double t)
{
    return ToDateTime(t, Qt::LocalTime).toString(Qt::LocaleDate);
}

static inline QString ToLocaleDateString(double t)
{
    return ToDateTime(t, Qt::LocalTime).date().toString(Qt::LocaleDate);
}

static inline QString ToLocaleTimeString(double t)
{
    return ToDateTime(t, Qt::LocalTime).time().toString(Qt::LocaleDate);
}

static double getLocalTZA()
{
#ifndef Q_OS_WIN
    struct tm t;
    time_t curr;
    tzset();
    time(&curr);
    localtime_r(&curr, &t);
    time_t locl = mktime(&t);
    gmtime_r(&curr, &t);
    time_t globl = mktime(&t);
    return (double(locl) - double(globl)) * 1000.0;
#else
    TIME_ZONE_INFORMATION tzInfo;
    GetTimeZoneInformation(&tzInfo);
    return -tzInfo.Bias * 60.0 * 1000.0;
#endif
}

DEFINE_OBJECT_VTABLE(DateObject);

void Heap::DateObject::init(const QDateTime &date)
{
    Object::init();
    this->date = date.isValid() ? date.toMSecsSinceEpoch() : qt_qnan();
}

void Heap::DateObject::init(const QTime &time)
{
    Object::init();
    if (!time.isValid()) {
        date = qt_qnan();
        return;
    }

    /* All programmers know that stuff starts at 0. Whatever that may mean in this context (and
     * local timezone), it's before the epoch, so there is defenitely no DST problem. Specifically:
     * you can't start with a date before the epoch, add some[*] hours, and end up with a date
     * after. That's a problem for timezones where new year happens during DST, like
     * Australia/Hobart, because we have to ignore DST before the epoch (but honor it after the
     * epoch).
     *
     * [*] Well, when "some" is in the range 0-24. If you add something like 1M then this might
     *     still happen.
     */
    static const double d = MakeDay(0, 0, 0);
    double t = MakeTime(time.hour(), time.minute(), time.second(), time.msec());
    date = TimeClip(UTC(MakeDate(d, t)));
}

QDateTime DateObject::toQDateTime() const
{
    return ToDateTime(date(), Qt::LocalTime);
}

DEFINE_OBJECT_VTABLE(DateCtor);

void Heap::DateCtor::init(QV4::ExecutionContext *scope)
{
    Heap::FunctionObject::init(scope, QStringLiteral("Date"));
}

void DateCtor::construct(const Managed *, Scope &scope, CallData *callData)
{
    double t = 0;

    if (callData->argc == 0)
        t = currentTime();

    else if (callData->argc == 1) {
        ScopedValue arg(scope, callData->args[0]);
        if (DateObject *d = arg->as<DateObject>()) {
            t = d->date();
        } else {
            arg = RuntimeHelpers::toPrimitive(arg, PREFERREDTYPE_HINT);

            if (arg->isString())
                t = ParseString(arg->stringValue()->toQString());
            else
                t = TimeClip(arg->toNumber());
        }
    }

    else { // d.argc > 1
        double year  = callData->args[0].toNumber();
        double month = callData->args[1].toNumber();
        double day  = callData->argc >= 3 ? callData->args[2].toNumber() : 1;
        double hours = callData->argc >= 4 ? callData->args[3].toNumber() : 0;
        double mins = callData->argc >= 5 ? callData->args[4].toNumber() : 0;
        double secs = callData->argc >= 6 ? callData->args[5].toNumber() : 0;
        double ms    = callData->argc >= 7 ? callData->args[6].toNumber() : 0;
        if (year >= 0 && year <= 99)
            year += 1900;
        t = MakeDate(MakeDay(year, month, day), MakeTime(hours, mins, secs, ms));
        t = TimeClip(UTC(t));
    }

    scope.result = Encode(scope.engine->newDateObject(Primitive::fromDouble(t)));
}

void DateCtor::call(const Managed *m, Scope &scope, CallData *)
{
    double t = currentTime();
    scope.result = static_cast<const DateCtor *>(m)->engine()->newString(ToString(t));
}

void DatePrototype::init(ExecutionEngine *engine, Object *ctor)
{
    Scope scope(engine);
    ScopedObject o(scope);
    ctor->defineReadonlyProperty(engine->id_prototype(), (o = this));
    ctor->defineReadonlyProperty(engine->id_length(), Primitive::fromInt32(7));
    LocalTZA = getLocalTZA();

    ctor->defineDefaultProperty(QStringLiteral("parse"), method_parse, 1);
    ctor->defineDefaultProperty(QStringLiteral("UTC"), method_UTC, 7);
    ctor->defineDefaultProperty(QStringLiteral("now"), method_now, 0);

    defineDefaultProperty(QStringLiteral("constructor"), (o = ctor));
    defineDefaultProperty(engine->id_toString(), method_toString, 0);
    defineDefaultProperty(QStringLiteral("toDateString"), method_toDateString, 0);
    defineDefaultProperty(QStringLiteral("toTimeString"), method_toTimeString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleDateString"), method_toLocaleDateString, 0);
    defineDefaultProperty(QStringLiteral("toLocaleTimeString"), method_toLocaleTimeString, 0);
    defineDefaultProperty(engine->id_valueOf(), method_valueOf, 0);
    defineDefaultProperty(QStringLiteral("getTime"), method_getTime, 0);
    defineDefaultProperty(QStringLiteral("getYear"), method_getYear, 0);
    defineDefaultProperty(QStringLiteral("getFullYear"), method_getFullYear, 0);
    defineDefaultProperty(QStringLiteral("getUTCFullYear"), method_getUTCFullYear, 0);
    defineDefaultProperty(QStringLiteral("getMonth"), method_getMonth, 0);
    defineDefaultProperty(QStringLiteral("getUTCMonth"), method_getUTCMonth, 0);
    defineDefaultProperty(QStringLiteral("getDate"), method_getDate, 0);
    defineDefaultProperty(QStringLiteral("getUTCDate"), method_getUTCDate, 0);
    defineDefaultProperty(QStringLiteral("getDay"), method_getDay, 0);
    defineDefaultProperty(QStringLiteral("getUTCDay"), method_getUTCDay, 0);
    defineDefaultProperty(QStringLiteral("getHours"), method_getHours, 0);
    defineDefaultProperty(QStringLiteral("getUTCHours"), method_getUTCHours, 0);
    defineDefaultProperty(QStringLiteral("getMinutes"), method_getMinutes, 0);
    defineDefaultProperty(QStringLiteral("getUTCMinutes"), method_getUTCMinutes, 0);
    defineDefaultProperty(QStringLiteral("getSeconds"), method_getSeconds, 0);
    defineDefaultProperty(QStringLiteral("getUTCSeconds"), method_getUTCSeconds, 0);
    defineDefaultProperty(QStringLiteral("getMilliseconds"), method_getMilliseconds, 0);
    defineDefaultProperty(QStringLiteral("getUTCMilliseconds"), method_getUTCMilliseconds, 0);
    defineDefaultProperty(QStringLiteral("getTimezoneOffset"), method_getTimezoneOffset, 0);
    defineDefaultProperty(QStringLiteral("setTime"), method_setTime, 1);
    defineDefaultProperty(QStringLiteral("setMilliseconds"), method_setMilliseconds, 1);
    defineDefaultProperty(QStringLiteral("setUTCMilliseconds"), method_setUTCMilliseconds, 1);
    defineDefaultProperty(QStringLiteral("setSeconds"), method_setSeconds, 2);
    defineDefaultProperty(QStringLiteral("setUTCSeconds"), method_setUTCSeconds, 2);
    defineDefaultProperty(QStringLiteral("setMinutes"), method_setMinutes, 3);
    defineDefaultProperty(QStringLiteral("setUTCMinutes"), method_setUTCMinutes, 3);
    defineDefaultProperty(QStringLiteral("setHours"), method_setHours, 4);
    defineDefaultProperty(QStringLiteral("setUTCHours"), method_setUTCHours, 4);
    defineDefaultProperty(QStringLiteral("setDate"), method_setDate, 1);
    defineDefaultProperty(QStringLiteral("setUTCDate"), method_setUTCDate, 1);
    defineDefaultProperty(QStringLiteral("setMonth"), method_setMonth, 2);
    defineDefaultProperty(QStringLiteral("setUTCMonth"), method_setUTCMonth, 2);
    defineDefaultProperty(QStringLiteral("setYear"), method_setYear, 1);
    defineDefaultProperty(QStringLiteral("setFullYear"), method_setFullYear, 3);
    defineDefaultProperty(QStringLiteral("setUTCFullYear"), method_setUTCFullYear, 3);
    defineDefaultProperty(QStringLiteral("toUTCString"), method_toUTCString, 0);
    defineDefaultProperty(QStringLiteral("toGMTString"), method_toUTCString, 0);
    defineDefaultProperty(QStringLiteral("toISOString"), method_toISOString, 0);
    defineDefaultProperty(QStringLiteral("toJSON"), method_toJSON, 1);
}

double DatePrototype::getThisDate(ExecutionContext *ctx)
{
    if (DateObject *thisObject = ctx->thisObject().as<DateObject>())
        return thisObject->date();
    else {
        ctx->engine()->throwTypeError();
        return 0;
    }
}

ReturnedValue DatePrototype::method_parse(CallContext *ctx)
{
    if (!ctx->argc())
        return Encode(qt_qnan());
    return Encode(ParseString(ctx->args()[0].toQString()));
}

ReturnedValue DatePrototype::method_UTC(CallContext *ctx)
{
    const int numArgs = ctx->argc();
    if (numArgs >= 2) {
        double year  = ctx->args()[0].toNumber();
        double month = ctx->args()[1].toNumber();
        double day   = numArgs >= 3 ? ctx->args()[2].toNumber() : 1;
        double hours = numArgs >= 4 ? ctx->args()[3].toNumber() : 0;
        double mins  = numArgs >= 5 ? ctx->args()[4].toNumber() : 0;
        double secs  = numArgs >= 6 ? ctx->args()[5].toNumber() : 0;
        double ms    = numArgs >= 7 ? ctx->args()[6].toNumber() : 0;
        if (year >= 0 && year <= 99)
            year += 1900;
        double t = MakeDate(MakeDay(year, month, day),
                            MakeTime(hours, mins, secs, ms));
        return Encode(TimeClip(t));
    }
    return Encode::undefined();
}

ReturnedValue DatePrototype::method_now(CallContext *ctx)
{
    Q_UNUSED(ctx);
    double t = currentTime();
    return Encode(t);
}

ReturnedValue DatePrototype::method_toString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_toDateString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToDateString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_toTimeString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToTimeString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_toLocaleString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToLocaleString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_toLocaleDateString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToLocaleDateString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_toLocaleTimeString(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return ctx->d()->engine->newString(ToLocaleTimeString(t))->asReturnedValue();
}

ReturnedValue DatePrototype::method_valueOf(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getTime(CallContext *ctx)
{
    double t = getThisDate(ctx);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getYear(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = YearFromTime(LocalTime(t)) - 1900;
    return Encode(t);
}

ReturnedValue DatePrototype::method_getFullYear(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = YearFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCFullYear(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = YearFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getMonth(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = MonthFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCMonth(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = MonthFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getDate(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = DateFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCDate(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = DateFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getDay(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = WeekDay(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCDay(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = WeekDay(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getHours(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = HourFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCHours(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = HourFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getMinutes(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = MinFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCMinutes(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = MinFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getSeconds(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = SecFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCSeconds(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = SecFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getMilliseconds(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = msFromTime(LocalTime(t));
    return Encode(t);
}

ReturnedValue DatePrototype::method_getUTCMilliseconds(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = msFromTime(t);
    return Encode(t);
}

ReturnedValue DatePrototype::method_getTimezoneOffset(CallContext *ctx)
{
    double t = getThisDate(ctx);
    if (! std::isnan(t))
        t = (t - LocalTime(t)) / msPerMinute;
    return Encode(t);
}

ReturnedValue DatePrototype::method_setTime(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<DateObject> self(scope, ctx->thisObject());
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    self->setDate(TimeClip(t));
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setMilliseconds(CallContext *ctx)
{
    Scope scope(ctx);
    Scoped<DateObject> self(scope, ctx->thisObject());
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double ms = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    self->setDate(TimeClip(UTC(MakeDate(Day(t), MakeTime(HourFromTime(t), MinFromTime(t), SecFromTime(t), ms)))));
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCMilliseconds(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double ms = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    self->setDate(TimeClip(MakeDate(Day(t), MakeTime(HourFromTime(t), MinFromTime(t), SecFromTime(t), ms))));
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setSeconds(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double sec = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double ms = (ctx->argc() < 2) ? msFromTime(t) : ctx->args()[1].toNumber();
    t = TimeClip(UTC(MakeDate(Day(t), MakeTime(HourFromTime(t), MinFromTime(t), sec, ms))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCSeconds(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double sec = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double ms = (ctx->argc() < 2) ? msFromTime(t) : ctx->args()[1].toNumber();
    t = TimeClip(MakeDate(Day(t), MakeTime(HourFromTime(t), MinFromTime(t), sec, ms)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setMinutes(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double min = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double sec = (ctx->argc() < 2) ? SecFromTime(t) : ctx->args()[1].toNumber();
    double ms = (ctx->argc() < 3) ? msFromTime(t) : ctx->args()[2].toNumber();
    t = TimeClip(UTC(MakeDate(Day(t), MakeTime(HourFromTime(t), min, sec, ms))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCMinutes(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double min = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double sec = (ctx->argc() < 2) ? SecFromTime(t) : ctx->args()[1].toNumber();
    double ms = (ctx->argc() < 3) ? msFromTime(t) : ctx->args()[2].toNumber();
    t = TimeClip(MakeDate(Day(t), MakeTime(HourFromTime(t), min, sec, ms)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setHours(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double hour = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double min = (ctx->argc() < 2) ? MinFromTime(t) : ctx->args()[1].toNumber();
    double sec = (ctx->argc() < 3) ? SecFromTime(t) : ctx->args()[2].toNumber();
    double ms = (ctx->argc() < 4) ? msFromTime(t) : ctx->args()[3].toNumber();
    t = TimeClip(UTC(MakeDate(Day(t), MakeTime(hour, min, sec, ms))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCHours(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double hour = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double min = (ctx->argc() < 2) ? MinFromTime(t) : ctx->args()[1].toNumber();
    double sec = (ctx->argc() < 3) ? SecFromTime(t) : ctx->args()[2].toNumber();
    double ms = (ctx->argc() < 4) ? msFromTime(t) : ctx->args()[3].toNumber();
    t = TimeClip(MakeDate(Day(t), MakeTime(hour, min, sec, ms)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setDate(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double date = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    t = TimeClip(UTC(MakeDate(MakeDay(YearFromTime(t), MonthFromTime(t), date), TimeWithinDay(t))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCDate(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double date = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    t = TimeClip(MakeDate(MakeDay(YearFromTime(t), MonthFromTime(t), date), TimeWithinDay(t)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setMonth(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    double month = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double date = (ctx->argc() < 2) ? DateFromTime(t) : ctx->args()[1].toNumber();
    t = TimeClip(UTC(MakeDate(MakeDay(YearFromTime(t), month, date), TimeWithinDay(t))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCMonth(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double month = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double date = (ctx->argc() < 2) ? DateFromTime(t) : ctx->args()[1].toNumber();
    t = TimeClip(MakeDate(MakeDay(YearFromTime(t), month, date), TimeWithinDay(t)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setYear(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    if (std::isnan(t))
        t = 0;
    else
        t = LocalTime(t);
    double year = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double r;
    if (std::isnan(year)) {
        r = qt_qnan();
    } else {
        if ((Primitive::toInteger(year) >= 0) && (Primitive::toInteger(year) <= 99))
            year += 1900;
        r = MakeDay(year, MonthFromTime(t), DateFromTime(t));
        r = UTC(MakeDate(r, TimeWithinDay(t)));
        r = TimeClip(r);
    }
    self->setDate(r);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setUTCFullYear(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    double year = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double month = (ctx->argc() < 2) ? MonthFromTime(t) : ctx->args()[1].toNumber();
    double date = (ctx->argc() < 3) ? DateFromTime(t) : ctx->args()[2].toNumber();
    t = TimeClip(MakeDate(MakeDay(year, month, date), TimeWithinDay(t)));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_setFullYear(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = LocalTime(self->date());
    if (std::isnan(t))
        t = 0;
    double year = ctx->argc() ? ctx->args()[0].toNumber() : qt_qnan();
    double month = (ctx->argc() < 2) ? MonthFromTime(t) : ctx->args()[1].toNumber();
    double date = (ctx->argc() < 3) ? DateFromTime(t) : ctx->args()[2].toNumber();
    t = TimeClip(UTC(MakeDate(MakeDay(year, month, date), TimeWithinDay(t))));
    self->setDate(t);
    return Encode(self->date());
}

ReturnedValue DatePrototype::method_toUTCString(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    return ctx->d()->engine->newString(ToUTCString(t))->asReturnedValue();
}

static void addZeroPrefixedInt(QString &str, int num, int nDigits)
{
    str.resize(str.size() + nDigits);

    QChar *c = str.data() + str.size() - 1;
    while (nDigits) {
        *c = QChar(num % 10 + '0');
        num /= 10;
        --c;
        --nDigits;
    }
}

ReturnedValue DatePrototype::method_toISOString(CallContext *ctx)
{
    DateObject *self = ctx->thisObject().as<DateObject>();
    if (!self)
        return ctx->engine()->throwTypeError();

    double t = self->date();
    if (!std::isfinite(t))
        return ctx->engine()->throwRangeError(ctx->thisObject());

    QString result;
    int year = (int)YearFromTime(t);
    if (year < 0 || year > 9999) {
        if (qAbs(year) >= 1000000)
            return ctx->d()->engine->newString(QStringLiteral("Invalid Date"))->asReturnedValue();
        result += year < 0 ? QLatin1Char('-') : QLatin1Char('+');
        year = qAbs(year);
        addZeroPrefixedInt(result, year, 6);
    } else {
        addZeroPrefixedInt(result, year, 4);
    }
    result += QLatin1Char('-');
    addZeroPrefixedInt(result, (int)MonthFromTime(t) + 1, 2);
    result += QLatin1Char('-');
    addZeroPrefixedInt(result, (int)DateFromTime(t), 2);
    result += QLatin1Char('T');
    addZeroPrefixedInt(result, HourFromTime(t), 2);
    result += QLatin1Char(':');
    addZeroPrefixedInt(result, MinFromTime(t), 2);
    result += QLatin1Char(':');
    addZeroPrefixedInt(result, SecFromTime(t), 2);
    result += QLatin1Char('.');
    addZeroPrefixedInt(result, msFromTime(t), 3);
    result += QLatin1Char('Z');

    return ctx->d()->engine->newString(result)->asReturnedValue();
}

ReturnedValue DatePrototype::method_toJSON(CallContext *ctx)
{
    Scope scope(ctx);
    ScopedValue O(scope, RuntimeHelpers::toObject(scope.engine, ctx->thisObject()));
    ScopedValue tv(scope, RuntimeHelpers::toPrimitive(O, NUMBER_HINT));

    if (tv->isNumber() && !std::isfinite(tv->toNumber()))
        return Encode::null();

    ScopedString s(scope, ctx->d()->engine->newString(QStringLiteral("toISOString")));
    ScopedValue v(scope, O->objectValue()->get(s));
    FunctionObject *toIso = v->as<FunctionObject>();

    if (!toIso)
        return ctx->engine()->throwTypeError();

    ScopedCallData callData(scope);
    callData->thisObject = ctx->thisObject();
    toIso->call(scope, callData);
    return scope.result.asReturnedValue();
}

void DatePrototype::timezoneUpdated()
{
    LocalTZA = getLocalTZA();
}
