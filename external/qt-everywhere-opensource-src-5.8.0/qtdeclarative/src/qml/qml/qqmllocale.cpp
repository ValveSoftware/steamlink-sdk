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

#include "qqmllocale_p.h"
#include "qqmlengine_p.h"
#include <private/qqmlcontext_p.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qdatetime.h>

#include <private/qlocale_p.h>
#include <private/qlocale_data_p.h>

#include <private/qv4dateobject_p.h>
#include <private/qv4numberobject_p.h>
#include <private/qv4stringobject_p.h>

QT_BEGIN_NAMESPACE

using namespace QV4;

DEFINE_OBJECT_VTABLE(QQmlLocaleData);

#define GET_LOCALE_DATA_RESOURCE(OBJECT) \
    QV4::Scoped<QQmlLocaleData> r(scope, OBJECT.as<QQmlLocaleData>()); \
    if (!r) \
        V4THROW_ERROR("Not a valid Locale object")

static bool isLocaleObject(const QV4::Value &val)
{
    return val.as<QQmlLocaleData>();
}

//--------------
// Date extension

void QQmlDateExtension::registerExtension(QV4::ExecutionEngine *engine)
{
    engine->datePrototype()->defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString);
    engine->datePrototype()->defineDefaultProperty(QStringLiteral("toLocaleTimeString"), method_toLocaleTimeString);
    engine->datePrototype()->defineDefaultProperty(QStringLiteral("toLocaleDateString"), method_toLocaleDateString);
    engine->dateCtor()->defineDefaultProperty(QStringLiteral("fromLocaleString"), method_fromLocaleString);
    engine->dateCtor()->defineDefaultProperty(QStringLiteral("fromLocaleTimeString"), method_fromLocaleTimeString);
    engine->dateCtor()->defineDefaultProperty(QStringLiteral("fromLocaleDateString"), method_fromLocaleDateString);
    engine->dateCtor()->defineDefaultProperty(QStringLiteral("timeZoneUpdated"), method_timeZoneUpdated);
}

QV4::ReturnedValue QQmlDateExtension::method_toLocaleString(QV4::CallContext *ctx)
{
    if (ctx->argc() > 2)
        return QV4::DatePrototype::method_toLocaleString(ctx);

    QV4::Scope scope(ctx);

    QV4::DateObject *date = ctx->thisObject().as<DateObject>();
    if (!date)
        return QV4::DatePrototype::method_toLocaleString(ctx);

    QDateTime dt = date->toQDateTime();

    if (ctx->argc() == 0) {
        // Use QLocale for standard toLocaleString() function
        QLocale locale;
        return ctx->d()->engine->newString(locale.toString(dt))->asReturnedValue();
    }

    if (!isLocaleObject(ctx->args()[0]))
        return QV4::DatePrototype::method_toLocaleString(ctx); // Use the default Date toLocaleString()

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QString formattedDt;
    if (ctx->argc() == 2) {
        if (ctx->args()[1].isString()) {
            QString format = ctx->args()[1].stringValue()->toQString();
            formattedDt = r->d()->locale->toString(dt, format);
        } else if (ctx->args()[1].isNumber()) {
            quint32 intFormat = ctx->args()[1].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            formattedDt = r->d()->locale->toString(dt, format);
        } else {
            V4THROW_ERROR("Locale: Date.toLocaleString(): Invalid datetime format");
        }
    } else {
         formattedDt = r->d()->locale->toString(dt, enumFormat);
    }

    return ctx->d()->engine->newString(formattedDt)->asReturnedValue();
}

QV4::ReturnedValue QQmlDateExtension::method_toLocaleTimeString(QV4::CallContext *ctx)
{
    if (ctx->argc() > 2)
        return QV4::DatePrototype::method_toLocaleTimeString(ctx);

    QV4::Scope scope(ctx);

    QV4::DateObject *date = ctx->thisObject().as<DateObject>();
    if (!date)
        return QV4::DatePrototype::method_toLocaleTimeString(ctx);

    QDateTime dt = date->toQDateTime();
    QTime time = dt.time();

    if (ctx->argc() == 0) {
        // Use QLocale for standard toLocaleString() function
        QLocale locale;
        return ctx->d()->engine->newString(locale.toString(time))->asReturnedValue();
    }

    if (!isLocaleObject(ctx->args()[0]))
        return QV4::DatePrototype::method_toLocaleTimeString(ctx); // Use the default Date toLocaleTimeString()

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QString formattedTime;
    if (ctx->argc() == 2) {
        if (ctx->args()[1].isString()) {
            QString format = ctx->args()[1].stringValue()->toQString();
            formattedTime = r->d()->locale->toString(time, format);
        } else if (ctx->args()[1].isNumber()) {
            quint32 intFormat = ctx->args()[1].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            formattedTime = r->d()->locale->toString(time, format);
        } else {
            V4THROW_ERROR("Locale: Date.toLocaleTimeString(): Invalid time format");
        }
    } else {
         formattedTime = r->d()->locale->toString(time, enumFormat);
    }

    return ctx->d()->engine->newString(formattedTime)->asReturnedValue();
}

QV4::ReturnedValue QQmlDateExtension::method_toLocaleDateString(QV4::CallContext *ctx)
{
    if (ctx->argc() > 2)
        return QV4::DatePrototype::method_toLocaleDateString(ctx);

    QV4::Scope scope(ctx);

    QV4::DateObject *dateObj = ctx->thisObject().as<DateObject>();
    if (!dateObj)
        return QV4::DatePrototype::method_toLocaleDateString(ctx);

    QDateTime dt = dateObj->toQDateTime();
    QDate date = dt.date();

    if (ctx->argc() == 0) {
        // Use QLocale for standard toLocaleString() function
        QLocale locale;
        return ctx->d()->engine->newString(locale.toString(date))->asReturnedValue();
    }

    if (!isLocaleObject(ctx->args()[0]))
        return QV4::DatePrototype::method_toLocaleDateString(ctx); // Use the default Date toLocaleDateString()

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QString formattedDate;
    if (ctx->argc() == 2) {
        if (ctx->args()[1].isString()) {
            QString format = ctx->args()[1].stringValue()->toQString();
            formattedDate = r->d()->locale->toString(date, format);
        } else if (ctx->args()[1].isNumber()) {
            quint32 intFormat = ctx->args()[1].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            formattedDate = r->d()->locale->toString(date, format);
        } else {
            V4THROW_ERROR("Locale: Date.loLocaleDateString(): Invalid date format");
        }
    } else {
         formattedDate = r->d()->locale->toString(date, enumFormat);
    }

    return ctx->d()->engine->newString(formattedDate)->asReturnedValue();
}

QV4::ReturnedValue QQmlDateExtension::method_fromLocaleString(QV4::CallContext *ctx)
{
    QV4::ExecutionEngine * const engine = ctx->d()->engine;
    if (ctx->argc() == 1 && ctx->args()[0].isString()) {
        QLocale locale;
        QString dateString = ctx->args()[0].stringValue()->toQString();
        QDateTime dt = locale.toDateTime(dateString);
        return QV4::Encode(engine->newDateObject(dt));
    }

    QV4::Scope scope(ctx);

    if (ctx->argc() < 1 || ctx->argc() > 3 || !isLocaleObject(ctx->args()[0]))
        V4THROW_ERROR("Locale: Date.fromLocaleString(): Invalid arguments");

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QDateTime dt;
    QString dateString = ctx->args()[1].toQStringNoThrow();
    if (ctx->argc() == 3) {
        if (ctx->args()[2].isString()) {
            QString format = ctx->args()[2].stringValue()->toQString();
            dt = r->d()->locale->toDateTime(dateString, format);
        } else if (ctx->args()[2].isNumber()) {
            quint32 intFormat = ctx->args()[2].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            dt = r->d()->locale->toDateTime(dateString, format);
        } else {
            V4THROW_ERROR("Locale: Date.fromLocaleString(): Invalid datetime format");
        }
    } else {
        dt = r->d()->locale->toDateTime(dateString, enumFormat);
    }

    return QV4::Encode(engine->newDateObject(dt));
}

QV4::ReturnedValue QQmlDateExtension::method_fromLocaleTimeString(QV4::CallContext *ctx)
{
    QV4::ExecutionEngine * const engine = ctx->d()->engine;

    if (ctx->argc() == 1 && ctx->args()[0].isString()) {
        QLocale locale;
        QString timeString = ctx->args()[0].stringValue()->toQString();
        QTime time = locale.toTime(timeString);
        QDateTime dt = QDateTime::currentDateTime();
        dt.setTime(time);
        return QV4::Encode(engine->newDateObject(dt));
    }

    if (ctx->argc() < 1 || ctx->argc() > 3 || !isLocaleObject(ctx->args()[0]))
        V4THROW_ERROR("Locale: Date.fromLocaleTimeString(): Invalid arguments");

    QV4::Scope scope(ctx);

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QTime tm;
    QString dateString = ctx->args()[1].toQStringNoThrow();
    if (ctx->argc() == 3) {
        if (ctx->args()[2].isString()) {
            QString format = ctx->args()[2].stringValue()->toQString();
            tm = r->d()->locale->toTime(dateString, format);
        } else if (ctx->args()[2].isNumber()) {
            quint32 intFormat = ctx->args()[2].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            tm = r->d()->locale->toTime(dateString, format);
        } else {
            V4THROW_ERROR("Locale: Date.fromLocaleTimeString(): Invalid datetime format");
        }
    } else {
        tm = r->d()->locale->toTime(dateString, enumFormat);
    }

    QDateTime dt;
    if (tm.isValid()) {
        dt = QDateTime::currentDateTime();
        dt.setTime(tm);
    }

    return QV4::Encode(engine->newDateObject(dt));
}

QV4::ReturnedValue QQmlDateExtension::method_fromLocaleDateString(QV4::CallContext *ctx)
{
    QV4::ExecutionEngine * const engine = ctx->d()->engine;

    if (ctx->argc() == 1 && ctx->args()[0].isString()) {
        QLocale locale;
        QString dateString = ctx->args()[0].stringValue()->toQString();
        QDate date = locale.toDate(dateString);
        return QV4::Encode(engine->newDateObject(QDateTime(date)));
    }

    if (ctx->argc() < 1 || ctx->argc() > 3 || !isLocaleObject(ctx->args()[0]))
        V4THROW_ERROR("Locale: Date.fromLocaleDateString(): Invalid arguments");

    QV4::Scope scope(ctx);

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QLocale::FormatType enumFormat = QLocale::LongFormat;
    QDate dt;
    QString dateString = ctx->args()[1].toQStringNoThrow();
    if (ctx->argc() == 3) {
        if (ctx->args()[2].isString()) {
            QString format = ctx->args()[2].stringValue()->toQString();
            dt = r->d()->locale->toDate(dateString, format);
        } else if (ctx->args()[2].isNumber()) {
            quint32 intFormat = ctx->args()[2].toNumber();
            QLocale::FormatType format = QLocale::FormatType(intFormat);
            dt = r->d()->locale->toDate(dateString, format);
        } else {
            V4THROW_ERROR("Locale: Date.fromLocaleDateString(): Invalid datetime format");
        }
    } else {
        dt = r->d()->locale->toDate(dateString, enumFormat);
    }

    return QV4::Encode(engine->newDateObject(QDateTime(dt)));
}

QV4::ReturnedValue QQmlDateExtension::method_timeZoneUpdated(QV4::CallContext *ctx)
{
    if (ctx->argc() != 0)
        V4THROW_ERROR("Locale: Date.timeZoneUpdated(): Invalid arguments");

    QV4::DatePrototype::timezoneUpdated();

    return QV4::Encode::undefined();
}

//-----------------
// Number extension

void QQmlNumberExtension::registerExtension(QV4::ExecutionEngine *engine)
{
    engine->numberPrototype()->defineDefaultProperty(QStringLiteral("toLocaleString"), method_toLocaleString);
    engine->numberPrototype()->defineDefaultProperty(QStringLiteral("toLocaleCurrencyString"), method_toLocaleCurrencyString);
    engine->numberCtor()->defineDefaultProperty(QStringLiteral("fromLocaleString"), method_fromLocaleString);
}

QV4::ReturnedValue QQmlNumberExtension::method_toLocaleString(QV4::CallContext *ctx)
{
    if (ctx->argc() > 3)
        V4THROW_ERROR("Locale: Number.toLocaleString(): Invalid arguments");

    double number = ctx->thisObject().toNumber();

    if (ctx->argc() == 0) {
        // Use QLocale for standard toLocaleString() function
        QLocale locale;
        return ctx->d()->engine->newString(locale.toString(number))->asReturnedValue();
    }

    if (!isLocaleObject(ctx->args()[0]))
        return QV4::NumberPrototype::method_toLocaleString(ctx); // Use the default Number toLocaleString()

    QV4::Scope scope(ctx);

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    quint16 format = 'f';
    if (ctx->argc() > 1) {
        if (!ctx->args()[1].isString())
            V4THROW_ERROR("Locale: Number.toLocaleString(): Invalid arguments");
        QString fs = ctx->args()[1].toQString();
        if (fs.length())
            format = fs.at(0).unicode();
    }
    int prec = 2;
    if (ctx->argc() > 2) {
        if (!ctx->args()[2].isNumber())
            V4THROW_ERROR("Locale: Number.toLocaleString(): Invalid arguments");
         prec = ctx->args()[2].toInt32();
    }

    return ctx->d()->engine->newString(r->d()->locale->toString(number, (char)format, prec))->asReturnedValue();
}

QV4::ReturnedValue QQmlNumberExtension::method_toLocaleCurrencyString(QV4::CallContext *ctx)
{
    if (ctx->argc() > 2)
        V4THROW_ERROR("Locale: Number.toLocaleCurrencyString(): Invalid arguments");

    double number = ctx->thisObject().toNumber();

    if (ctx->argc() == 0) {
        // Use QLocale for standard toLocaleString() function
        QLocale locale;
        return ctx->d()->engine->newString(locale.toString(number))->asReturnedValue();
    }

    if (!isLocaleObject(ctx->args()[0]))
        V4THROW_ERROR("Locale: Number.toLocaleCurrencyString(): Invalid arguments");

    QV4::Scope scope(ctx);

    GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);

    QString symbol;
    if (ctx->argc() > 1) {
        if (!ctx->args()[1].isString())
            V4THROW_ERROR("Locale: Number.toLocaleString(): Invalid arguments");
        symbol = ctx->args()[1].toQStringNoThrow();
    }

    return ctx->d()->engine->newString(r->d()->locale->toCurrencyString(number, symbol))->asReturnedValue();
}

QV4::ReturnedValue QQmlNumberExtension::method_fromLocaleString(QV4::CallContext *ctx)
{
    if (ctx->argc() < 1 || ctx->argc() > 2)
        V4THROW_ERROR("Locale: Number.fromLocaleString(): Invalid arguments");

    int numberIdx = 0;
    QLocale locale;

    QV4::Scope scope(ctx);

    if (ctx->argc() == 2) {
        if (!isLocaleObject(ctx->args()[0]))
            V4THROW_ERROR("Locale: Number.fromLocaleString(): Invalid arguments");

        GET_LOCALE_DATA_RESOURCE(ctx->args()[0]);
        locale = *r->d()->locale;

        numberIdx = 1;
    }

    QString ns = ctx->args()[numberIdx].toQString();
    if (!ns.length())
        return QV4::Encode(Q_QNAN);

    bool ok = false;
    double val = locale.toDouble(ns, &ok);

    if (!ok)
        V4THROW_ERROR("Locale: Number.fromLocaleString(): Invalid format")

    return QV4::Encode(val);
}

//--------------
// Locale object

QV4::ReturnedValue QQmlLocaleData::method_get_firstDayOfWeek(QV4::CallContext *ctx)
{
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();
    int fdow = int(locale->firstDayOfWeek());
    if (fdow == 7)
        fdow = 0; // Qt::Sunday = 7, but Sunday is 0 in JS Date
    return QV4::Encode(fdow);
}

QV4::ReturnedValue QQmlLocaleData::method_get_measurementSystem(QV4::CallContext *ctx)
{
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();
    return QV4::Encode(locale->measurementSystem());
}

QV4::ReturnedValue QQmlLocaleData::method_get_textDirection(QV4::CallContext *ctx)
{
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();

    return QV4::Encode(locale->textDirection());
}

QV4::ReturnedValue QQmlLocaleData::method_get_weekDays(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();

    QList<Qt::DayOfWeek> days = locale->weekdays();

    QV4::ScopedArrayObject result(scope, ctx->d()->engine->newArrayObject());
    result->arrayReserve(days.size());
    for (int i = 0; i < days.size(); ++i) {
        int day = days.at(i);
        if (day == 7) // JS Date days in range 0(Sunday) to 6(Saturday)
            day = 0;
        result->arrayPut(i, QV4::Primitive::fromInt32(day));
    }
    result->setArrayLengthUnchecked(days.size());

    return result.asReturnedValue();
}

QV4::ReturnedValue QQmlLocaleData::method_get_uiLanguages(QV4::CallContext *ctx)
{
    QV4::Scope scope(ctx);
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();

    QStringList langs = locale->uiLanguages();
    QV4::ScopedArrayObject result(scope, ctx->d()->engine->newArrayObject());
    result->arrayReserve(langs.size());
    QV4::ScopedValue v(scope);
    for (int i = 0; i < langs.size(); ++i)
        result->arrayPut(i, (v = ctx->d()->engine->newString(langs.at(i))));

    result->setArrayLengthUnchecked(langs.size());

    return result.asReturnedValue();
}

QV4::ReturnedValue QQmlLocaleData::method_currencySymbol(QV4::CallContext *ctx)
{
    QLocale *locale = getThisLocale(ctx);
    if (!locale)
        return QV4::Encode::undefined();

    if (ctx->argc() > 1)
        V4THROW_ERROR("Locale: currencySymbol(): Invalid arguments");

    QLocale::CurrencySymbolFormat format = QLocale::CurrencySymbol;
    if (ctx->argc() == 1) {
        quint32 intFormat = ctx->args()[0].toNumber();
        format = QLocale::CurrencySymbolFormat(intFormat);
    }

    return ctx->d()->engine->newString(locale->currencySymbol(format))->asReturnedValue();
}

#define LOCALE_FORMAT(FUNC) \
QV4::ReturnedValue QQmlLocaleData::method_ ##FUNC (QV4::CallContext *ctx) { \
    QLocale *locale = getThisLocale(ctx); \
    if (!locale) \
        return QV4::Encode::undefined(); \
    if (ctx->argc() > 1) \
        V4THROW_ERROR("Locale: " #FUNC "(): Invalid arguments"); \
    QLocale::FormatType format = QLocale::LongFormat;\
    if (ctx->argc() == 1) { \
        quint32 intFormat = ctx->args()[0].toUInt32(); \
        format = QLocale::FormatType(intFormat); \
    } \
    return ctx->engine()->newString(locale-> FUNC (format))->asReturnedValue(); \
}

LOCALE_FORMAT(dateTimeFormat)
LOCALE_FORMAT(timeFormat)
LOCALE_FORMAT(dateFormat)

// +1 added to idx because JS is 0-based, whereas QLocale months begin at 1.
#define LOCALE_FORMATTED_MONTHNAME(VARIABLE) \
QV4::ReturnedValue QQmlLocaleData::method_ ## VARIABLE (QV4::CallContext *ctx) {\
    QLocale *locale = getThisLocale(ctx); \
    if (!locale) \
        return QV4::Encode::undefined(); \
    if (ctx->argc() < 1 || ctx->argc() > 2) \
        V4THROW_ERROR("Locale: " #VARIABLE "(): Invalid arguments"); \
    QLocale::FormatType enumFormat = QLocale::LongFormat; \
    int idx = ctx->args()[0].toInt32() + 1; \
    if (idx < 1 || idx > 12) \
        V4THROW_ERROR("Locale: Invalid month"); \
    QString name; \
    if (ctx->argc() == 2) { \
        if (ctx->args()[1].isNumber()) { \
            quint32 intFormat = ctx->args()[1].toUInt32(); \
            QLocale::FormatType format = QLocale::FormatType(intFormat); \
            name = locale-> VARIABLE(idx, format); \
        } else { \
            V4THROW_ERROR("Locale: Invalid datetime format"); \
        } \
    } else { \
        name = locale-> VARIABLE(idx, enumFormat); \
    } \
    return ctx->engine()->newString(name)->asReturnedValue(); \
}

// 0 -> 7 as Qt::Sunday is 7, but Sunday is 0 in JS Date
#define LOCALE_FORMATTED_DAYNAME(VARIABLE) \
QV4::ReturnedValue QQmlLocaleData::method_ ## VARIABLE (QV4::CallContext *ctx) {\
    QLocale *locale = getThisLocale(ctx); \
    if (!locale) \
        return QV4::Encode::undefined(); \
    if (ctx->argc() < 1 || ctx->argc() > 2) \
        V4THROW_ERROR("Locale: " #VARIABLE "(): Invalid arguments"); \
    QLocale::FormatType enumFormat = QLocale::LongFormat; \
    int idx = ctx->args()[0].toInt32(); \
    if (idx < 0 || idx > 7) \
        V4THROW_ERROR("Locale: Invalid day"); \
    if (idx == 0) idx = 7; \
    QString name; \
    if (ctx->argc() == 2) { \
        if (ctx->args()[1].isNumber()) { \
            quint32 intFormat = ctx->args()[1].toUInt32(); \
            QLocale::FormatType format = QLocale::FormatType(intFormat); \
            name = locale-> VARIABLE(idx, format); \
        } else { \
            V4THROW_ERROR("Locale: Invalid datetime format"); \
        } \
    } else { \
        name = locale-> VARIABLE(idx, enumFormat); \
    } \
    return ctx->engine()->newString(name)->asReturnedValue(); \
}

LOCALE_FORMATTED_MONTHNAME(monthName)
LOCALE_FORMATTED_MONTHNAME(standaloneMonthName)
LOCALE_FORMATTED_DAYNAME(dayName)
LOCALE_FORMATTED_DAYNAME(standaloneDayName)

#define LOCALE_STRING_PROPERTY(VARIABLE) QV4::ReturnedValue QQmlLocaleData::method_get_ ## VARIABLE (QV4::CallContext* ctx) \
{ \
    QLocale *locale = getThisLocale(ctx); \
    if (!locale) \
        return QV4::Encode::undefined(); \
    return ctx->engine()->newString(locale-> VARIABLE())->asReturnedValue();\
}

LOCALE_STRING_PROPERTY(name)
LOCALE_STRING_PROPERTY(nativeLanguageName)
LOCALE_STRING_PROPERTY(nativeCountryName)
LOCALE_STRING_PROPERTY(decimalPoint)
LOCALE_STRING_PROPERTY(groupSeparator)
LOCALE_STRING_PROPERTY(percent)
LOCALE_STRING_PROPERTY(zeroDigit)
LOCALE_STRING_PROPERTY(negativeSign)
LOCALE_STRING_PROPERTY(positiveSign)
LOCALE_STRING_PROPERTY(exponential)
LOCALE_STRING_PROPERTY(amText)
LOCALE_STRING_PROPERTY(pmText)

class QV4LocaleDataDeletable : public QV8Engine::Deletable
{
public:
    QV4LocaleDataDeletable(QV4::ExecutionEngine *engine);
    ~QV4LocaleDataDeletable();

    QV4::PersistentValue prototype;
};

QV4LocaleDataDeletable::QV4LocaleDataDeletable(QV4::ExecutionEngine *engine)
{
    QV4::Scope scope(engine);
    QV4::Scoped<QV4::Object> o(scope, engine->newObject());

    o->defineDefaultProperty(QStringLiteral("dateFormat"), QQmlLocaleData::method_dateFormat, 0);
    o->defineDefaultProperty(QStringLiteral("standaloneDayName"), QQmlLocaleData::method_standaloneDayName, 0);
    o->defineDefaultProperty(QStringLiteral("standaloneMonthName"), QQmlLocaleData::method_standaloneMonthName, 0);
    o->defineDefaultProperty(QStringLiteral("dayName"), QQmlLocaleData::method_dayName, 0);
    o->defineDefaultProperty(QStringLiteral("timeFormat"), QQmlLocaleData::method_timeFormat, 0);
    o->defineDefaultProperty(QStringLiteral("monthName"), QQmlLocaleData::method_monthName, 0);
    o->defineDefaultProperty(QStringLiteral("currencySymbol"), QQmlLocaleData::method_currencySymbol, 0);
    o->defineDefaultProperty(QStringLiteral("dateTimeFormat"), QQmlLocaleData::method_dateTimeFormat, 0);
    o->defineAccessorProperty(QStringLiteral("name"), QQmlLocaleData::method_get_name, 0);
    o->defineAccessorProperty(QStringLiteral("positiveSign"), QQmlLocaleData::method_get_positiveSign, 0);
    o->defineAccessorProperty(QStringLiteral("uiLanguages"), QQmlLocaleData::method_get_uiLanguages, 0);
    o->defineAccessorProperty(QStringLiteral("firstDayOfWeek"), QQmlLocaleData::method_get_firstDayOfWeek, 0);
    o->defineAccessorProperty(QStringLiteral("pmText"), QQmlLocaleData::method_get_pmText, 0);
    o->defineAccessorProperty(QStringLiteral("percent"), QQmlLocaleData::method_get_percent, 0);
    o->defineAccessorProperty(QStringLiteral("textDirection"), QQmlLocaleData::method_get_textDirection, 0);
    o->defineAccessorProperty(QStringLiteral("weekDays"), QQmlLocaleData::method_get_weekDays, 0);
    o->defineAccessorProperty(QStringLiteral("negativeSign"), QQmlLocaleData::method_get_negativeSign, 0);
    o->defineAccessorProperty(QStringLiteral("groupSeparator"), QQmlLocaleData::method_get_groupSeparator, 0);
    o->defineAccessorProperty(QStringLiteral("decimalPoint"), QQmlLocaleData::method_get_decimalPoint, 0);
    o->defineAccessorProperty(QStringLiteral("nativeLanguageName"), QQmlLocaleData::method_get_nativeLanguageName, 0);
    o->defineAccessorProperty(QStringLiteral("nativeCountryName"), QQmlLocaleData::method_get_nativeCountryName, 0);
    o->defineAccessorProperty(QStringLiteral("zeroDigit"), QQmlLocaleData::method_get_zeroDigit, 0);
    o->defineAccessorProperty(QStringLiteral("amText"), QQmlLocaleData::method_get_amText, 0);
    o->defineAccessorProperty(QStringLiteral("measurementSystem"), QQmlLocaleData::method_get_measurementSystem, 0);
    o->defineAccessorProperty(QStringLiteral("exponential"), QQmlLocaleData::method_get_exponential, 0);

    prototype.set(engine, o);
}

QV4LocaleDataDeletable::~QV4LocaleDataDeletable()
{
}

V4_DEFINE_EXTENSION(QV4LocaleDataDeletable, localeV4Data);

/*!
    \qmltype Locale
    \instantiates QQmlLocale
    \inqmlmodule QtQml
    \brief Provides locale specific properties and formatted data

    The Locale object may only be created via the \l{QtQml::Qt::locale()}{Qt.locale()} function.
    It cannot be created directly.

    The \l{QtQml::Qt::locale()}{Qt.locale()} function returns a JS Locale object representing the
    locale with the specified name, which has the format
    "language[_territory][.codeset][@modifier]" or "C".

    Locale supports the concept of a default locale, which is
    determined from the system's locale settings at application
    startup.  If no parameter is passed to Qt.locale() the default
    locale object is returned.

    The Locale object provides a number of functions and properties
    providing data for the specified locale.

    The Locale object may also be passed to the \l Date and \l Number toLocaleString()
    and fromLocaleString() methods in order to convert to/from strings using
    the specified locale.

    This example shows the current date formatted for the German locale:

    \code
    import QtQuick 2.0

    Text {
        text: "The date is: " + Date().toLocaleString(Qt.locale("de_DE"))
    }
    \endcode

    The following example displays the specified number
    in the correct format for the default locale:

    \code
    import QtQuick 2.0

    Text {
        text: "The value is: " + Number(23443.34).toLocaleString(Qt.locale())
    }
    \endcode

    Qt Quick Locale's data is based on Common Locale Data Repository v1.8.1.


    \target FormatType
    \section2 Locale String Format Types

    The monthName(), standaloneMonthName(), dayName() and standaloneDayName()
    can use the following enumeration values to specify the formatting of
    the string representation for a Date object.

    \list
    \li Locale.LongFormat The long version of day and month names; for
    example, returning "January" as a month name.
    \li Locale.ShortFormat The short version of day and month names; for
    example, returning "Jan" as a month name.
    \li Locale.NarrowFormat A special version of day and month names for
    use when space is limited; for example, returning "J" as a month
    name. Note that the narrow format might contain the same text for
    different months and days or it can even be an empty string if the
    locale doesn't support narrow names, so you should avoid using it
    for date formatting. Also, for the system locale this format is
    the same as ShortFormat.
    \endlist


    Additionally the double-to-string and string-to-double conversion functions are
    covered by the following licenses:

    \legalese
    Copyright (c) 1991 by AT&T.

    Permission to use, copy, modify, and distribute this software for any
    purpose without fee is hereby granted, provided that this entire notice
    is included in all copies of any software which is or includes a copy
    or modification of this software and in all copies of the supporting
    documentation for such software.

    THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
    WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR AT&T MAKES ANY
    REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
    OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.

    This product includes software developed by the University of
    California, Berkeley and its contributors.

    \sa Date, Number
*/

QQmlLocale::QQmlLocale()
{
}

QQmlLocale::~QQmlLocale()
{
}

QV4::ReturnedValue QQmlLocale::locale(ExecutionEngine *engine, const QString &localeName)
{
    QLocale qlocale;
    if (!localeName.isEmpty())
        qlocale = localeName;
    return wrap(engine, qlocale);
}

QV4::ReturnedValue QQmlLocale::wrap(ExecutionEngine *v4, const QLocale &locale)
{
    QV4::Scope scope(v4);
    QV4LocaleDataDeletable *d = localeV4Data(scope.engine);
    QV4::Scoped<QQmlLocaleData> wrapper(scope, v4->memoryManager->allocObject<QQmlLocaleData>());
    *wrapper->d()->locale = locale;
    QV4::ScopedObject p(scope, d->prototype.value());
    wrapper->setPrototype(p);
    return wrapper.asReturnedValue();
}

void QQmlLocale::registerStringLocaleCompare(QV4::ExecutionEngine *engine)
{
    engine->stringPrototype()->defineDefaultProperty(QStringLiteral("localeCompare"), method_localeCompare);
}

QV4::ReturnedValue QQmlLocale::method_localeCompare(QV4::CallContext *ctx)
{
    if (ctx->argc() != 1 || (!ctx->args()[0].isString() && !ctx->args()[0].as<StringObject>()))
        return QV4::StringPrototype::method_localeCompare(ctx);

    if (!ctx->thisObject().isString() && !ctx->thisObject().as<StringObject>())
        return QV4::StringPrototype::method_localeCompare(ctx);

    QString thisString = ctx->thisObject().toQStringNoThrow();
    QString thatString = ctx->args()[0].toQStringNoThrow();

    return QV4::Encode(QString::localeAwareCompare(thisString, thatString));
}

/*!
    \qmlproperty string QtQml::Locale::name

    Holds the language and country of this locale as a
    string of the form "language_country", where
    language is a lowercase, two-letter ISO 639 language code,
    and country is an uppercase, two- or three-letter ISO 3166 country code.
*/

/*!
    \qmlproperty string QtQml::Locale::decimalPoint

    Holds the decimal point character of this locale.
*/

/*!
    \qmlproperty string QtQml::Locale::groupSeparator

    Holds the group separator character of this locale.
*/

/*!
    \qmlproperty string QtQml::Locale::percent

    Holds the percent character of this locale.
*/


/*!
    \qmlproperty string QtQml::Locale::zeroDigit

    Holds Returns the zero digit character of this locale.
*/

/*!
    \qmlproperty string QtQml::Locale::negativeSign

    Holds the negative sign character of this locale.
*/

/*!
    \qmlproperty string QtQml::Locale::positiveSign

    Holds the positive sign character of this locale.
*/

/*!
    \qmlproperty string QtQml::Locale::exponential

    Holds the exponential character of this locale.
*/

/*!
    \qmlmethod string QtQml::Locale::dateTimeFormat(type)

    Returns the date time format used for the current locale.
    \a type specifies the FormatType to return.

    \sa Date
*/

/*!
    \qmlmethod string QtQml::Locale::dateFormat(type)

    Returns the date format used for the current locale.
    \a type specifies the FormatType to return.

    \sa Date
*/

/*!
    \qmlmethod string QtQml::Locale::timeFormat(type)

    Returns the time format used for the current locale.
    \a type specifies the FormatType to return.

    \sa Date
*/

/*!
    \qmlmethod string QtQml::Locale::monthName(month, type)

    Returns the localized name of \a month (0-11), in the optional
    \l FormatType specified by \a type.

    \note the QLocale C++ API expects a range of (1-12), however Locale.monthName()
    expects 0-11 as per the JS Date object.

    \sa dayName(), standaloneMonthName()
*/

/*!
    \qmlmethod string QtQml::Locale::standaloneMonthName(month, type)

    Returns the localized name of \a month (0-11) that is used as a
    standalone text, in the optional \l FormatType specified by \a type.

    If the locale information doesn't specify the standalone month
    name then return value is the same as in monthName().

    \note the QLocale C++ API expects a range of (1-12), however Locale.standaloneMonthName()
    expects 0-11 as per the JS Date object.

    \sa monthName(), standaloneDayName()
*/

/*!
    \qmlmethod string QtQml::Locale::dayName(day, type)

    Returns the localized name of the \a day (where 0 represents
    Sunday, 1 represents Monday and so on), in the optional
    \l FormatType specified by \a type.

    \sa monthName(), standaloneDayName()
*/

/*!
    \qmlmethod string QtQml::Locale::standaloneDayName(day, type)

    Returns the localized name of the \a day (where 0 represents
    Sunday, 1 represents Monday and so on) that is used as a
    standalone text, in the \l FormatType specified by \a type.

    If the locale information does not specify the standalone day
    name then return value is the same as in dayName().

    \sa dayName(), standaloneMonthName()
*/

/*!
    \qmlproperty enumeration QtQml::Locale::firstDayOfWeek

    Holds the first day of the week according to the current locale.

    \list
    \li Locale.Sunday = 0
    \li Locale.Monday = 1
    \li Locale.Tuesday = 2
    \li Locale.Wednesday = 3
    \li Locale.Thursday = 4
    \li Locale.Friday = 5
    \li Locale.Saturday = 6
    \endlist

    \note that these values match the JS Date API which is different
    from the Qt C++ API where Qt::Sunday = 7.
*/

/*!
    \qmlproperty Array<int> QtQml::Locale::weekDays

    Holds an array of days that are considered week days according to the current locale,
    where Sunday is 0 and Saturday is 6.

    \sa firstDayOfWeek
*/

/*!
    \qmlproperty Array<string> QtQml::Locale::uiLanguages

    Returns an ordered list of locale names for translation purposes in
    preference order.

    The return value represents locale names that the user expects to see the
    UI translation in.

    The first item in the list is the most preferred one.
*/

/*!
    \qmlproperty enumeration QtQml::Locale::textDirection

    Holds the text direction of the language:
    \list
    \li Qt.LeftToRight
    \li Qt.RightToLeft
    \endlist
*/

/*!
    \qmlproperty string QtQml::Locale::amText

    The localized name of the "AM" suffix for times specified using the conventions of the 12-hour clock.
*/

/*!
    \qmlproperty string QtQml::Locale::pmText

    The localized name of the "PM" suffix for times specified using the conventions of the 12-hour clock.
*/

/*!
    \qmlmethod string QtQml::Locale::currencySymbol(format)

    Returns the currency symbol for the specified \a format:
    \list
    \li Locale.CurrencyIsoCode a ISO-4217 code of the currency.
    \li Locale.CurrencySymbol a currency symbol.
    \li Locale.CurrencyDisplayName a user readable name of the currency.
    \endlist
    \sa Number::toLocaleCurrencyString()
*/

/*!
    \qmlproperty string QtQml::Locale::nativeLanguageName

    Holds a native name of the language for the locale. For example
    "Schwiizertüütsch" for Swiss-German locale.

    \sa nativeCountryName
*/

/*!
    \qmlproperty string QtQml::Locale::nativeCountryName

    Holds a native name of the country for the locale. For example
    "España" for Spanish/Spain locale.

    \sa nativeLanguageName
*/

/*!
    \qmlproperty enumeration QtQml::Locale::measurementSystem

    This property defines which units are used for measurement.

    \list
    \li Locale.MetricSystem This value indicates metric units, such as meters,
        centimeters and millimeters.
    \li Locale.ImperialUSSystem This value indicates imperial units, such as
        inches and miles as they are used in the United States.
    \li Locale.ImperialUKSystem This value indicates imperial units, such as
        inches and miles as they are used in the United Kingdom.
    \li Locale.ImperialSystem Provided for compatibility. The same as
        Locale.ImperialUSSystem.
    \endlist
*/

QT_END_NAMESPACE
