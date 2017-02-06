/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Calendar module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQml/qqmlextensionplugin.h>

#include "qquickdayofweekrow_p.h"
#include "qquickmonthgrid_p.h"
#include "qquickweeknumbercolumn_p.h"
#include "qquickcalendarmodel_p.h"
#include "qquickcalendar_p.h"

static inline void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_Qt_labs_calendar);
#endif
}

QT_BEGIN_NAMESPACE

class QtLabsCalendarPlugin: public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtLabsCalendarPlugin(QObject *parent = nullptr);
    void registerTypes(const char *uri);
};

QtLabsCalendarPlugin::QtLabsCalendarPlugin(QObject *parent) : QQmlExtensionPlugin(parent)
{
    initResources();
}

static QObject *calendarSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return new QQuickCalendar;
}

void QtLabsCalendarPlugin::registerTypes(const char *uri)
{
    qmlRegisterType<QQuickDayOfWeekRow>(uri, 1, 0, "AbstractDayOfWeekRow");
    qmlRegisterType<QQuickMonthGrid>(uri, 1, 0, "AbstractMonthGrid");
    qmlRegisterType<QQuickWeekNumberColumn>(uri, 1, 0, "AbstractWeekNumberColumn");
    qmlRegisterType<QQuickCalendarModel>(uri, 1, 0, "CalendarModel");
    qmlRegisterSingletonType<QQuickCalendar>(uri, 1, 0, "Calendar", calendarSingleton);
}

QT_END_NAMESPACE

#include "qtlabscalendarplugin.moc"
