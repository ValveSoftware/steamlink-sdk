/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qtest.h>
#include <QDebug>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtCore/QDateTime>
#include <qcolor.h>
#include "../../shared/util.h"

#include <time.h>

class tst_qqmllocale : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmllocale() { }

private slots:
    void defaultLocale();

    void properties_data();
    void properties();
    void currencySymbol_data();
    void currencySymbol();
    void monthName_data();
    void monthName();
    void standaloneMonthName_data();
    void standaloneMonthName();
    void dayName_data();
    void dayName();
    void standaloneDayName_data();
    void standaloneDayName();
    void firstDayOfWeek_data();
    void firstDayOfWeek();
    void weekDays_data();
    void weekDays();
    void uiLanguages_data();
    void uiLanguages();
    void dateFormat_data();
    void dateFormat();
    void dateTimeFormat_data();
    void dateTimeFormat();
    void timeFormat_data();
    void timeFormat();
#if defined(Q_OS_UNIX)
    void timeZoneUpdated();
#endif

    void dateToLocaleString_data();
    void dateToLocaleString();
    void dateToLocaleStringFormatted_data();
    void dateToLocaleStringFormatted();
    void dateToLocaleDateString_data();
    void dateToLocaleDateString();
    void dateToLocaleDateStringFormatted_data();
    void dateToLocaleDateStringFormatted();
    void dateToLocaleTimeString_data();
    void dateToLocaleTimeString();
    void dateToLocaleTimeStringFormatted_data();
    void dateToLocaleTimeStringFormatted();
    void dateFromLocaleString_data();
    void dateFromLocaleString();
    void dateFromLocaleDateString_data();
    void dateFromLocaleDateString();
    void dateFromLocaleTimeString_data();
    void dateFromLocaleTimeString();

    void numberToLocaleString_data();
    void numberToLocaleString();
    void numberToLocaleCurrencyString_data();
    void numberToLocaleCurrencyString();
    void numberFromLocaleString_data();
    void numberFromLocaleString();
    void numberConstToLocaleString();

    void stringLocaleCompare_data();
    void stringLocaleCompare();

    void localeAsCppProperty();
private:
    void addPropertyData(const QString &l);
    QVariant getProperty(QObject *obj, const QString &locale, const QString &property);
    void addCurrencySymbolData(const QString &locale);
    void addStandardFormatData();
    void addFormatNameData(const QString &locale);
    void addDateTimeFormatData(const QString &l);
    void addDateFormatData(const QString &l);
    void addTimeFormatData(const QString &l);
    QQmlEngine engine;
};

void tst_qqmllocale::defaultLocale()
{
    QQmlComponent c(&engine, testFileUrl("properties.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QCOMPARE(obj->property("name").toString(), QLocale().name());
}

#define LOCALE_PROP(type,prop) { #prop, QVariant(type(qlocale.prop())) }

void tst_qqmllocale::addPropertyData(const QString &l)
{
    QLocale qlocale(l);

    struct {
        const char *name;
        QVariant value;
    }
    values[] = {
        LOCALE_PROP(QString,name),
        LOCALE_PROP(QString,amText),
        LOCALE_PROP(QString,pmText),
        LOCALE_PROP(QString,nativeLanguageName),
        LOCALE_PROP(QString,nativeCountryName),
        LOCALE_PROP(QString,decimalPoint),
        LOCALE_PROP(QString,groupSeparator),
        LOCALE_PROP(QString,percent),
        LOCALE_PROP(QString,zeroDigit),
        LOCALE_PROP(QString,negativeSign),
        LOCALE_PROP(QString,positiveSign),
        LOCALE_PROP(QString,exponential),
        LOCALE_PROP(int,measurementSystem),
        LOCALE_PROP(int,textDirection),
        { 0, QVariant() }
    };

    int i = 0;
    while (values[i].name) {
        QByteArray n = l.toLatin1() + ':' + values[i].name;
        QTest::newRow(n.constData()) << l << QByteArray(values[i].name) << values[i].value;
        ++i;
    }
}

void tst_qqmllocale::properties_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QByteArray>("property");
    QTest::addColumn<QVariant>("value");

    addPropertyData("en_US");
    addPropertyData("de_DE");
    addPropertyData("ar_SA");
    addPropertyData("hi_IN");
    addPropertyData("zh_CN");
    addPropertyData("th_TH");
}

void tst_qqmllocale::properties()
{
    QFETCH(QString, locale);
    QFETCH(QByteArray, property);
    QFETCH(QVariant, value);

    QQmlComponent c(&engine, testFileUrl("properties.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QCOMPARE(obj->property(property), value);

    delete obj;
}

void tst_qqmllocale::addCurrencySymbolData(const QString &l)
{
    QByteArray locale = l.toLatin1();
    QTest::newRow(locale.constData()) << l << -1;
    QByteArray t(locale);
    t += " CurrencyIsoCode";
    QTest::newRow(t.constData()) << l << (int)QLocale::CurrencyIsoCode;
    t = locale + " CurrencySymbol";
    QTest::newRow(t.constData()) << l << (int)QLocale::CurrencySymbol;
    t = locale + " CurrencyDisplayName";
    QTest::newRow(t.constData()) << l << (int)QLocale::CurrencyDisplayName;
}

void tst_qqmllocale::currencySymbol_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<int>("param");

    addCurrencySymbolData("en_US");
    addCurrencySymbolData("de_DE");
    addCurrencySymbolData("ar_SA");
    addCurrencySymbolData("hi_IN");
    addCurrencySymbolData("zh_CN");
    addCurrencySymbolData("th_TH");
}

void tst_qqmllocale::currencySymbol()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;
    QLocale::CurrencySymbolFormat format = QLocale::CurrencySymbol;

    if (param >= 0)
        format = QLocale::CurrencySymbolFormat(param);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QMetaObject::invokeMethod(obj, "currencySymbol", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(int(format))));

    QCOMPARE(val.toString(), l.currencySymbol(format));

    delete obj;
}

void tst_qqmllocale::addFormatNameData(const QString &l)
{
    QByteArray locale = l.toLatin1();
    QTest::newRow(locale.constData()) << l << -1;
    QByteArray t(locale);
    t += " LongFormat";
    QTest::newRow(t.constData()) << l << (int)QLocale::LongFormat;
    t = locale + " ShortFormat";
    QTest::newRow(t.constData()) << l << (int)QLocale::ShortFormat;
    t = locale + " NarrowFormat";
    QTest::newRow(t.constData()) << l << (int)QLocale::NarrowFormat;
}

void tst_qqmllocale::addStandardFormatData()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<int>("param");

    addFormatNameData("en_US");
    addFormatNameData("de_DE");
    addFormatNameData("ar_SA");
    addFormatNameData("hi_IN");
    addFormatNameData("zh_CN");
    addFormatNameData("th_TH");
}

void tst_qqmllocale::monthName_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::monthName()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;
    QLocale::FormatType format = QLocale::LongFormat;
    if (param >= 0)
        format = QLocale::FormatType(param);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    for (int i = 0; i <= 11; ++i) {
        QMetaObject::invokeMethod(obj, "monthName", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(i)),
            Q_ARG(QVariant, QVariant(int(format))));

        // QLocale January == 1, JS Date January == 0
        QCOMPARE(val.toString(), l.monthName(i+1, format));
    }

    delete obj;
}

void tst_qqmllocale::standaloneMonthName_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::standaloneMonthName()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;
    QLocale::FormatType format = QLocale::LongFormat;
    if (param >= 0)
        format = QLocale::FormatType(param);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    for (int i = 0; i <= 11; ++i) {
        QMetaObject::invokeMethod(obj, "standaloneMonthName", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(i)),
            Q_ARG(QVariant, QVariant(int(format))));

        // QLocale January == 1, JS Date January == 0
        QCOMPARE(val.toString(), l.standaloneMonthName(i+1, format));
    }

    delete obj;
}

void tst_qqmllocale::dayName_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dayName()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;
    QLocale::FormatType format = QLocale::LongFormat;
    if (param >= 0)
        format = QLocale::FormatType(param);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    for (int i = 1; i <= 7; ++i) {
        QMetaObject::invokeMethod(obj, "dayName", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(i)),
            Q_ARG(QVariant, QVariant(int(format))));

        QCOMPARE(val.toString(), l.dayName(i, format));
    }

    delete obj;
}

void tst_qqmllocale::standaloneDayName_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::standaloneDayName()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;
    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    for (int i = 1; i <= 7; ++i) {
        QMetaObject::invokeMethod(obj, "standaloneDayName", Qt::DirectConnection,
                Q_RETURN_ARG(QVariant, val),
                Q_ARG(QVariant, QVariant(i)),
                Q_ARG(QVariant, QVariant(int(format))));

        QCOMPARE(val.toString(), l.standaloneDayName(i, format));
    }

    delete obj;
}

void tst_qqmllocale::firstDayOfWeek_data()
{
    QTest::addColumn<QString>("locale");

    QTest::newRow("en_US") << "en_US";
    QTest::newRow("de_DE") << "de_DE";
    QTest::newRow("ar_SA") << "ar_SA";
    QTest::newRow("hi_IN") << "hi_IN";
    QTest::newRow("zh_CN") << "zh_CN";
    QTest::newRow("th_TH") << "th_TH";
}

void tst_qqmllocale::firstDayOfWeek()
{
    QFETCH(QString, locale);

    QQmlComponent c(&engine, testFileUrl("properties.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val = obj->property("firstDayOfWeek");
    QCOMPARE(val.type(), QVariant::Int);

    int day = int(QLocale(locale).firstDayOfWeek());
    if (day == 7) // JS Date days in range 0(Sunday) to 6(Saturday)
        day = 0;
    QCOMPARE(day, val.toInt());

    delete obj;
}

void tst_qqmllocale::weekDays_data()
{
    QTest::addColumn<QString>("locale");

    QTest::newRow("en_US") << "en_US";
    QTest::newRow("de_DE") << "de_DE";
    QTest::newRow("ar_SA") << "ar_SA";
    QTest::newRow("hi_IN") << "hi_IN";
    QTest::newRow("zh_CN") << "zh_CN";
    QTest::newRow("th_TH") << "th_TH";
}

void tst_qqmllocale::weekDays()
{
    QFETCH(QString, locale);

    QQmlComponent c(&engine, testFileUrl("properties.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val = obj->property("weekDays");
    QCOMPARE(val.userType(), qMetaTypeId<QJSValue>());

    QList<QVariant> qmlDays = val.toList();
    QList<Qt::DayOfWeek> days = QLocale(locale).weekdays();

    QCOMPARE(days.count(), qmlDays.count());

    for (int i = 0; i < days.count(); ++i) {
        int day = int(days.at(i));
        if (day == 7) // JS Date days in range 0(Sunday) to 6(Saturday)
            day = 0;
        QCOMPARE(day, qmlDays.at(i).toInt());
    }

    delete obj;
}

void tst_qqmllocale::uiLanguages_data()
{
    QTest::addColumn<QString>("locale");

    QTest::newRow("en_US") << "en_US";
    QTest::newRow("de_DE") << "de_DE";
    QTest::newRow("ar_SA") << "ar_SA";
    QTest::newRow("hi_IN") << "hi_IN";
    QTest::newRow("zh_CN") << "zh_CN";
    QTest::newRow("th_TH") << "th_TH";
}

void tst_qqmllocale::uiLanguages()
{
    QFETCH(QString, locale);

    QQmlComponent c(&engine, testFileUrl("properties.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val = obj->property("uiLanguages");
    QCOMPARE(val.userType(), qMetaTypeId<QJSValue>());

    QList<QVariant> qmlLangs = val.toList();
    QStringList langs = QLocale(locale).uiLanguages();

    QCOMPARE(langs.count(), qmlLangs.count());

    for (int i = 0; i < langs.count(); ++i) {
        QCOMPARE(langs.at(i), qmlLangs.at(i).toString());
    }

    delete obj;
}


void tst_qqmllocale::dateTimeFormat_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dateTimeFormat()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);
    QMetaObject::invokeMethod(obj, "dateTimeFormat", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(param)));

    QCOMPARE(val.toString(), l.dateTimeFormat(format));
}

void tst_qqmllocale::dateFormat_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dateFormat()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);
    QMetaObject::invokeMethod(obj, "dateFormat", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(param)));

    QCOMPARE(val.toString(), l.dateFormat(format));
}

void tst_qqmllocale::timeFormat_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::timeFormat()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("functions.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QVariant val;

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);
    QMetaObject::invokeMethod(obj, "timeFormat", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, val),
            Q_ARG(QVariant, QVariant(param)));

    QCOMPARE(val.toString(), l.timeFormat(format));
}

void tst_qqmllocale::dateToLocaleString_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dateToLocaleString()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(param)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt, format));
}

void tst_qqmllocale::addDateTimeFormatData(const QString &l)
{
    const char *formats[] = {
        "hh:mm dd.MM.yyyy",
        "h:m:sap ddd MMMM d yy",
        "'The date and time is: 'H:mm:ss:zzz dd/MM/yy",
        "MMM d yyyy HH:mm t",
        0
    };
    QByteArray locale = l.toLatin1();
    int i = 0;
    while (formats[i]) {
        QByteArray t(locale);
        t += ' ';
        t += formats[i];
        QTest::newRow(t.constData()) << l << QString(formats[i]);
        ++i;
    }
}

void tst_qqmllocale::dateToLocaleStringFormatted_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    addDateTimeFormatData("en_US");
    addDateTimeFormatData("de_DE");
    addDateTimeFormatData("ar_SA");
    addDateTimeFormatData("hi_IN");
    addDateTimeFormatData("zh_CN");
    addDateTimeFormatData("th_TH");
}

void tst_qqmllocale::dateToLocaleStringFormatted()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(format)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt, format));
}

void tst_qqmllocale::dateToLocaleDateString_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dateToLocaleDateString()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleDateString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(param)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt.date(), format));
}

void tst_qqmllocale::addDateFormatData(const QString &l)
{
    const char *formats[] = {
        "dd.MM.yyyy",
        "ddd MMMM d yy",
        "'The date is: 'dd/MM/yy",
        "MMM d yyyy",
        0
    };
    QByteArray locale = l.toLatin1();
    int i = 0;
    while (formats[i]) {
        QByteArray t(locale);
        t += ' ';
        t += formats[i];
        QTest::newRow(t.constData()) << l << QString(formats[i]);
        ++i;
    }
}

void tst_qqmllocale::dateToLocaleDateStringFormatted_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    addDateFormatData("en_US");
    addDateFormatData("de_DE");
    addDateFormatData("ar_SA");
    addDateFormatData("hi_IN");
    addDateFormatData("zh_CN");
    addDateFormatData("th_TH");
}

void tst_qqmllocale::dateToLocaleDateStringFormatted()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(format)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt.date(), format));
}

void tst_qqmllocale::dateToLocaleTimeString_data()
{
    addStandardFormatData();
}

void tst_qqmllocale::dateToLocaleTimeString()
{
    QFETCH(QString, locale);
    QFETCH(int, param);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale::FormatType format = param < 0 ? QLocale::LongFormat : QLocale::FormatType(param);

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleTimeString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(param)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt.time(), format));
}

void tst_qqmllocale::addTimeFormatData(const QString &l)
{
    const char *formats[] = {
        "hh:mm",
        "h:m:sap",
        "'The time is: 'H:mm:ss:zzz",
        "HH:mm t",
        0
    };
    QByteArray locale = l.toLatin1();
    int i = 0;
    while (formats[i]) {
        QByteArray t(locale);
        t += ' ';
        t += formats[i];
        QTest::newRow(t.constData()) << l << QString(formats[i]);
        ++i;
    }
}

void tst_qqmllocale::dateToLocaleTimeStringFormatted_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    addTimeFormatData("en_US");
    addTimeFormatData("de_DE");
    addTimeFormatData("ar_SA");
    addTimeFormatData("hi_IN");
    addTimeFormatData("zh_CN");
    addTimeFormatData("th_TH");
}

void tst_qqmllocale::dateToLocaleTimeStringFormatted()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7)); // weirdly, JS Date month range is 0-11
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(format)));

    QLocale l(locale);
    QCOMPARE(val.toString(), l.toString(dt, format));
}

void tst_qqmllocale::dateFromLocaleString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    QTest::newRow("en_US 1") << "en_US" << "dddd, MMMM d, yyyy h:mm:ss AP";
    QTest::newRow("en_US long") << "en_US" << QLocale("en_US").dateTimeFormat();
    QTest::newRow("en_US short") << "en_US" << QLocale("en_US").dateTimeFormat(QLocale::ShortFormat);
    QTest::newRow("de_DE long") << "de_DE" << QLocale("de_DE").dateTimeFormat();
    QTest::newRow("de_DE short") << "de_DE" << QLocale("de_DE").dateTimeFormat(QLocale::ShortFormat);
    QTest::newRow("ar_SA long") << "ar_SA" << QLocale("ar_SA").dateTimeFormat();
    QTest::newRow("ar_SA short") << "ar_SA" << QLocale("ar_SA").dateTimeFormat(QLocale::ShortFormat);
    QTest::newRow("zh_CN long") << "zh_CN" << QLocale("zh_CN").dateTimeFormat();
    QTest::newRow("zh_CN short") << "zh_CN" << QLocale("zh_CN").dateTimeFormat(QLocale::ShortFormat);
}

void tst_qqmllocale::dateFromLocaleString()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7));
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale l(locale);
    QVariant val;
    QMetaObject::invokeMethod(obj, "fromLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(l.toString(dt, format))),
        Q_ARG(QVariant, QVariant(format)));

    QDateTime pd = l.toDateTime(l.toString(dt, format), format);
    QCOMPARE(val.toDateTime(), pd);
}

void tst_qqmllocale::dateFromLocaleDateString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    QTest::newRow("en_US 1") << "en_US" << "dddd, MMMM d, yyyy h:mm:ss AP";
    QTest::newRow("en_US long") << "en_US" << QLocale("en_US").dateFormat();
    QTest::newRow("en_US short") << "en_US" << QLocale("en_US").dateFormat(QLocale::ShortFormat);
    QTest::newRow("de_DE long") << "de_DE" << QLocale("de_DE").dateFormat();
    QTest::newRow("de_DE short") << "de_DE" << QLocale("de_DE").dateFormat(QLocale::ShortFormat);
    QTest::newRow("ar_SA long") << "ar_SA" << QLocale("ar_SA").dateFormat();
    QTest::newRow("ar_SA short") << "ar_SA" << QLocale("ar_SA").dateFormat(QLocale::ShortFormat);
    QTest::newRow("zh_CN long") << "zh_CN" << QLocale("zh_CN").dateFormat();
    QTest::newRow("zh_CN short") << "zh_CN" << QLocale("zh_CN").dateFormat(QLocale::ShortFormat);
}

void tst_qqmllocale::dateFromLocaleDateString()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7));
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale l(locale);
    QVariant val;
    QMetaObject::invokeMethod(obj, "fromLocaleDateString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(l.toString(dt, format))),
        Q_ARG(QVariant, QVariant(format)));

    QDate pd = l.toDate(l.toString(dt, format), format);
    QCOMPARE(val.toDate(), pd);
}

void tst_qqmllocale::dateFromLocaleTimeString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("format");

    QTest::newRow("en_US 1") << "en_US" << "dddd, MMMM d, yyyy h:mm:ss AP";
    QTest::newRow("en_US long") << "en_US" << QLocale("en_US").timeFormat();
    QTest::newRow("en_US short") << "en_US" << QLocale("en_US").timeFormat(QLocale::ShortFormat);
    QTest::newRow("de_DE long") << "de_DE" << QLocale("de_DE").timeFormat();
    QTest::newRow("de_DE short") << "de_DE" << QLocale("de_DE").timeFormat(QLocale::ShortFormat);
    QTest::newRow("ar_SA long") << "ar_SA" << QLocale("ar_SA").timeFormat();
    QTest::newRow("ar_SA short") << "ar_SA" << QLocale("ar_SA").timeFormat(QLocale::ShortFormat);
    QTest::newRow("zh_CN long") << "zh_CN" << QLocale("zh_CN").timeFormat();
    QTest::newRow("zh_CN short") << "zh_CN" << QLocale("zh_CN").timeFormat(QLocale::ShortFormat);
}

void tst_qqmllocale::dateFromLocaleTimeString()
{
    QFETCH(QString, locale);
    QFETCH(QString, format);

    QQmlComponent c(&engine, testFileUrl("date.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QDateTime dt;
    dt.setDate(QDate(2011, 10, 7));
    dt.setTime(QTime(18, 53, 48, 345));

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale l(locale);
    QVariant val;
    QMetaObject::invokeMethod(obj, "fromLocaleTimeString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(l.toString(dt, format))),
        Q_ARG(QVariant, QVariant(format)));

    QTime pd = l.toTime(l.toString(dt, format), format);
    QCOMPARE(val.toTime(), pd);
}

void tst_qqmllocale::numberToLocaleString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<char>("format");
    QTest::addColumn<int>("prec");

    QTest::newRow("en_US 1") << "en_US" << 'f' << 2;
    QTest::newRow("en_US 2") << "en_US" << 'g' << 3;
    QTest::newRow("en_US 3") << "en_US" << 'f' << 0;
    QTest::newRow("en_US 4") << "en_US" << 'f' << -1;
    QTest::newRow("de_DE 1") << "de_DE" << 'f' << 2;
    QTest::newRow("de_DE 2") << "de_DE" << 'g' << 3;
    QTest::newRow("ar_SA 1") << "ar_SA" << 'f' << 2;
    QTest::newRow("ar_SA 2") << "ar_SA" << 'g' << 3;
}

void tst_qqmllocale::numberToLocaleString()
{
    QFETCH(QString, locale);
    QFETCH(char, format);
    QFETCH(int, prec);

    QQmlComponent c(&engine, testFileUrl("number.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    double number = 2344423.3289;

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale l(locale);
    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(number)),
        Q_ARG(QVariant, QVariant(QString(format))),
        Q_ARG(QVariant, QVariant(prec)));

    if (prec < 0) prec = 2;
    QCOMPARE(val.toString(), l.toString(number, format, prec));
}

void tst_qqmllocale::numberToLocaleCurrencyString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<QString>("symbol");

    QTest::newRow("en_US 1") << "en_US" << QString();
    QTest::newRow("en_US 2") << "en_US" << "USD";
    QTest::newRow("de_DE") << "de_DE" << QString();
    QTest::newRow("ar_SA") << "ar_SA" << QString();
    QTest::newRow("hi_IN") << "hi_IN" << QString();
    QTest::newRow("zh_CN") << "zh_CN" << QString();
    QTest::newRow("th_TH") << "th_TH" << QString();
}

void tst_qqmllocale::numberToLocaleCurrencyString()
{
    QFETCH(QString, locale);
    QFETCH(QString, symbol);

    QQmlComponent c(&engine, testFileUrl("number.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    double number = 2344423.3289;

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QLocale l(locale);
    QVariant val;
    QMetaObject::invokeMethod(obj, "toLocaleCurrencyString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(number)),
        Q_ARG(QVariant, QVariant(symbol)));

    QCOMPARE(val.toString(), l.toCurrencyString(number, symbol));
}

void tst_qqmllocale::numberFromLocaleString_data()
{
    QTest::addColumn<QString>("locale");
    QTest::addColumn<double>("number");

    QTest::newRow("en_US 1") << "en_US" << 1234567.2345;
    QTest::newRow("en_US 2") << "en_US" << 0.234;
    QTest::newRow("en_US 3") << "en_US" << 234.0;
    QTest::newRow("de_DE") << "de_DE" << 1234567.2345;
    QTest::newRow("ar_SA") << "ar_SA" << 1234567.2345;
    QTest::newRow("hi_IN") << "hi_IN" << 1234567.2345;
    QTest::newRow("zh_CN") << "zh_CN" << 1234567.2345;
    QTest::newRow("th_TH") << "th_TH" << 1234567.2345;
}

void tst_qqmllocale::numberFromLocaleString()
{
    QFETCH(QString, locale);
    QFETCH(double, number);

    QQmlComponent c(&engine, testFileUrl("number.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QLocale l(locale);
    QString strNumber = l.toString(number, 'f');

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant(locale)));

    QVariant val;
    QMetaObject::invokeMethod(obj, "fromLocaleString", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, val),
        Q_ARG(QVariant, QVariant(strNumber)));

    QCOMPARE(val.toDouble(), l.toDouble(strNumber));
}

void tst_qqmllocale::numberConstToLocaleString()
{
    QQmlComponent c(&engine, testFileUrl("number.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    QMetaObject::invokeMethod(obj, "setLocale", Qt::DirectConnection,
        Q_ARG(QVariant, QVariant("en_US")));

    QLocale l("en_US");
    QCOMPARE(obj->property("const1").toString(), l.toString(1234.56, 'f', 2));
    QCOMPARE(obj->property("const2").toString(), l.toString(1234., 'f', 2));
}

void tst_qqmllocale::stringLocaleCompare_data()
{
    QTest::addColumn<QString>("string1");
    QTest::addColumn<QString>("string2");

    QTest::newRow("before") << "a" << "b";
    QTest::newRow("equal") << "a" << "a";
    QTest::newRow("after") << "b" << "a";

    // Copied from QString::localeAwareCompare tests
    // We don't actually change locale - we just care that String.localeCompare()
    // matches QString::localeAwareCompare();
    QTest::newRow("swedish1") << QString::fromLatin1("\xe5") << QString::fromLatin1("\xe4");
    QTest::newRow("swedish2") << QString::fromLatin1("\xe4") << QString::fromLatin1("\xf6");
    QTest::newRow("swedish3") << QString::fromLatin1("\xe5") << QString::fromLatin1("\xf6");
    QTest::newRow("swedish4") << QString::fromLatin1("z") << QString::fromLatin1("\xe5");

    QTest::newRow("german1") << QString::fromLatin1("z") << QString::fromLatin1("\xe4");
    QTest::newRow("german2") << QString::fromLatin1("\xe4") << QString::fromLatin1("\xf6");
    QTest::newRow("german3") << QString::fromLatin1("z") << QString::fromLatin1("\xf6");
}

void tst_qqmllocale::stringLocaleCompare()
{
    QFETCH(QString, string1);
    QFETCH(QString, string2);

    QQmlComponent c(&engine, testFileUrl("localeCompare.qml"));

    QObject *obj = c.create();
    QVERIFY(obj);

    obj->setProperty("string1", string1);
    obj->setProperty("string2", string2);

    QCOMPARE(obj->property("comparison").toInt(), QString::localeAwareCompare(string1, string2));
}

class Calendar : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)
public:
    Calendar() {
    }

    QLocale locale() const {
        return mLocale;
    }

    void setLocale(const QLocale &locale) {
        mLocale = locale;
    }
private:
    QLocale mLocale;
};

void tst_qqmllocale::localeAsCppProperty()
{
    QQmlComponent component(&engine);
    qmlRegisterType<Calendar>("Test", 1, 0, "Calendar");
    component.setData("import QtQml 2.2\nimport Test 1.0\nCalendar { locale: Qt.locale('en_GB'); property var testLocale }", QUrl());
    QVERIFY(!component.isError());
    QTRY_VERIFY(component.isReady());

    Calendar *item = qobject_cast<Calendar*>(component.create());
    QCOMPARE(item->property("locale").toLocale().name(), QLatin1String("en_GB"));

    QVariant localeVariant(QLocale("nb_NO"));
    item->setProperty("testLocale", localeVariant);
    QCOMPARE(item->property("testLocale").toLocale().name(), QLatin1String("nb_NO"));
}

class DateFormatter : public QObject
{
    Q_OBJECT
public:
    DateFormatter() : QObject() {}

    Q_INVOKABLE QString getLocalizedForm(const QString &isoTimestamp);
};

QString DateFormatter::getLocalizedForm(const QString &isoTimestamp)
{
    QDateTime input = QDateTime::fromString(isoTimestamp, Qt::ISODate);
    QLocale locale;
    return locale.toString(input);
}

#if defined(Q_OS_UNIX)
// Currently disabled on Windows as adjusting the timezone
// requires additional privileges that aren't normally
// enabled for a process. This can be achieved by calling
// AdjustTokenPrivileges() and then SetTimeZoneInformation(),
// which will require linking to a different library to access that API.
static void setTimeZone(const QByteArray &tz)
{
    qputenv("TZ", tz);
    ::tzset();

// following left for future reference, see comment above
// #if defined(Q_OS_WIN32)
//     ::_tzset();
// #endif
}

void tst_qqmllocale::timeZoneUpdated()
{
    QByteArray original(qgetenv("TZ"));

    // Set the timezone to Brisbane time
    setTimeZone(QByteArray("AEST-10:00"));

    DateFormatter formatter;

    QQmlEngine e;
    e.rootContext()->setContextObject(&formatter);

    QQmlComponent c(&e, testFileUrl("timeZoneUpdated.qml"));
    QScopedPointer<QObject> obj(c.create());
    QVERIFY(obj);
    QCOMPARE(obj->property("success").toBool(), true);

    // Change to Indian time
    setTimeZone(QByteArray("IST-05:30"));

    QMetaObject::invokeMethod(obj.data(), "check");

    // Reset to original time
    setTimeZone(original);
    QMetaObject::invokeMethod(obj.data(), "resetTimeZone");

    QCOMPARE(obj->property("success").toBool(), true);
}
#endif

QTEST_MAIN(tst_qqmllocale)

#include "tst_qqmllocale.moc"
