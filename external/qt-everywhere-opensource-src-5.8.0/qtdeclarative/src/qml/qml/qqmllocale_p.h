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

#ifndef QQMLLOCALE_H
#define QQMLLOCALE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qqml.h>

#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <private/qqmlglobal_p.h>
#include <private/qv4object_p.h>

QT_BEGIN_NAMESPACE


class QQmlDateExtension
{
public:
    static void registerExtension(QV4::ExecutionEngine *engine);

private:
    static QV4::ReturnedValue method_toLocaleString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_toLocaleTimeString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_toLocaleDateString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fromLocaleString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fromLocaleTimeString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fromLocaleDateString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_timeZoneUpdated(QV4::CallContext *ctx);
};


class QQmlNumberExtension
{
public:
    static void registerExtension(QV4::ExecutionEngine *engine);

private:
    static QV4::ReturnedValue method_toLocaleString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_fromLocaleString(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_toLocaleCurrencyString(QV4::CallContext *ctx);
};


class Q_QML_PRIVATE_EXPORT QQmlLocale
{
    Q_GADGET

public:
    ~QQmlLocale();

    enum MeasurementSystem {
        MetricSystem = QLocale::MetricSystem,
        ImperialSystem = QLocale::ImperialSystem,
        ImperialUSSystem = QLocale::ImperialUSSystem,
        ImperialUKSystem = QLocale::ImperialUKSystem
    };
    Q_ENUM(MeasurementSystem)
    enum FormatType {
        LongFormat = QLocale::LongFormat,
        ShortFormat = QLocale::ShortFormat,
        NarrowFormat = QLocale::NarrowFormat
    };
    Q_ENUM(FormatType)
    enum CurrencySymbolFormat {
        CurrencyIsoCode = QLocale::CurrencyIsoCode,
        CurrencySymbol = QLocale::CurrencySymbol,
        CurrencyDisplayName = QLocale::CurrencyDisplayName
    };
    Q_ENUM(CurrencySymbolFormat)
    // Qt defines Sunday as 7, but JS Date assigns Sunday 0
    enum DayOfWeek {
        Sunday = 0,
        Monday = Qt::Monday,
        Tuesday = Qt::Tuesday,
        Wednesday = Qt::Wednesday,
        Thursday = Qt::Thursday,
        Friday = Qt::Friday,
        Saturday = Qt::Saturday
    };
    Q_ENUM(DayOfWeek)

    static QV4::ReturnedValue locale(QV4::ExecutionEngine *engine, const QString &localeName);
    static QV4::ReturnedValue wrap(QV4::ExecutionEngine *engine, const QLocale &locale);

    static void registerStringLocaleCompare(QV4::ExecutionEngine *engine);

private:
    QQmlLocale();

    static QV4::ReturnedValue method_localeCompare(QV4::CallContext *ctx);
};

namespace QV4 {

namespace Heap {

struct QQmlLocaleData : Object {
    inline void init() { locale = new QLocale; }
    void destroy() {
        delete locale;
        Object::destroy();
    }
    QLocale *locale;
};

}

struct QQmlLocaleData : public QV4::Object
{
    V4_OBJECT2(QQmlLocaleData, Object)
    V4_NEEDS_DESTROY

    static QLocale *getThisLocale(QV4::CallContext *ctx) {
        QV4::Object *o = ctx->thisObject().as<Object>();
        QQmlLocaleData *thisObject = o ? o->as<QQmlLocaleData>() : 0;
        if (!thisObject) {
            ctx->engine()->throwTypeError();
            return 0;
        }
        return thisObject->d()->locale;
    }

    static QV4::ReturnedValue method_currencySymbol(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_dateTimeFormat(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_timeFormat(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_dateFormat(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_monthName(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_standaloneMonthName(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_dayName(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_standaloneDayName(QV4::CallContext *ctx);

    static QV4::ReturnedValue method_get_firstDayOfWeek(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_measurementSystem(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_textDirection(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_weekDays(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_uiLanguages(QV4::CallContext *ctx);

    static QV4::ReturnedValue method_get_name(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_nativeLanguageName(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_nativeCountryName(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_decimalPoint(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_groupSeparator(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_percent(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_zeroDigit(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_negativeSign(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_positiveSign(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_exponential(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_amText(QV4::CallContext *ctx);
    static QV4::ReturnedValue method_get_pmText(QV4::CallContext *ctx);
};

}

QT_END_NAMESPACE

#endif
