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

#include "qquickdayofweekrow_p.h"
#include "qquickdayofweekmodel_p.h"

#include <QtQuickTemplates2/private/qquickcontrol_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype DayOfWeekRow
    \inherits Control
    \instantiates QQuickDayOfWeekRow
    \inqmlmodule Qt.labs.calendar
    \brief A row of names for the days in a week.

    DayOfWeekRow presents day of week names in a row. The names of the days
    are ordered and formatted using the specified \l {Control::locale}{locale}.

    \image qtlabscalendar-dayofweekrow.png
    \snippet qtlabscalendar-dayofweekrow.qml 1

    DayOfWeekRow can be used as a standalone control, but it is most
    often used in conjunction with MonthGrid. Regardless of the use case,
    positioning of the row is left to the user.

    \image qtlabscalendar-dayofweekrow-layout.png
    \snippet qtlabscalendar-dayofweekrow-layout.qml 1

    The visual appearance of DayOfWeekRow can be changed by
    implementing a \l {delegate}{custom delegate}.

    \labs

    \sa MonthGrid, WeekNumberColumn
*/

class QQuickDayOfWeekRowPrivate : public QQuickControlPrivate
{
public:
    QQuickDayOfWeekRowPrivate() : delegate(nullptr), model(nullptr) { }

    void resizeItems();

    QVariant source;
    QQmlComponent *delegate;
    QQuickDayOfWeekModel *model;
};

void QQuickDayOfWeekRowPrivate::resizeItems()
{
    if (!contentItem)
        return;

    QSizeF itemSize;
    itemSize.setWidth((contentItem->width() - 6 * spacing) / 7);
    itemSize.setHeight(contentItem->height());

    const auto childItems = contentItem->childItems();
    for (QQuickItem *item : childItems)
        item->setSize(itemSize);
}

QQuickDayOfWeekRow::QQuickDayOfWeekRow(QQuickItem *parent) :
    QQuickControl(*(new QQuickDayOfWeekRowPrivate), parent)
{
    Q_D(QQuickDayOfWeekRow);
    d->model = new QQuickDayOfWeekModel(this);
    d->source = QVariant::fromValue(d->model);
}

/*!
    \internal
    \qmlproperty model Qt.labs.calendar::DayOfWeekRow::source

    This property holds the source model that is used as a data model
    for the internal content row.
*/
QVariant QQuickDayOfWeekRow::source() const
{
    Q_D(const QQuickDayOfWeekRow);
    return d->source;
}

void QQuickDayOfWeekRow::setSource(const QVariant &source)
{
    Q_D(QQuickDayOfWeekRow);
    if (d->source != source) {
        d->source = source;
        emit sourceChanged();
    }
}

/*!
    \qmlproperty Component Qt.labs.calendar::DayOfWeekRow::delegate

    This property holds the item delegate that visualizes each day of the week.

    In addition to the \c index property, a list of model data roles
    are available in the context of each delegate:
    \table
        \row \li \b model.day : int \li The day of week (\l Qt::DayOfWeek)
        \row \li \b model.longName : string \li The long version of the day name; for example, "Monday" (\l QLocale::LongFormat)
        \row \li \b model.shortName : string \li The short version of the day name; for example, "Mon" (\l QLocale::ShortFormat)
        \row \li \b model.narrowName : string \li A special version of the day name for use when space is limited; for example, "M" (\l QLocale::NarrowFormat)
    \endtable

    The following snippet presents the default implementation of the item
    delegate. It can be used as a starting point for implementing custom
    delegates.

    \snippet DayOfWeekRow.qml delegate
*/
QQmlComponent *QQuickDayOfWeekRow::delegate() const
{
    Q_D(const QQuickDayOfWeekRow);
    return d->delegate;
}

void QQuickDayOfWeekRow::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickDayOfWeekRow);
    if (d->delegate != delegate) {
        d->delegate = delegate;
        emit delegateChanged();
    }
}

void QQuickDayOfWeekRow::componentComplete()
{
    Q_D(QQuickDayOfWeekRow);
    QQuickControl::componentComplete();
    d->resizeItems();
}

void QQuickDayOfWeekRow::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickDayOfWeekRow);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    if (isComponentComplete())
        d->resizeItems();
}

void QQuickDayOfWeekRow::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_D(QQuickDayOfWeekRow);
    QQuickControl::localeChange(newLocale, oldLocale);
    d->model->setLocale(newLocale);
}

void QQuickDayOfWeekRow::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickDayOfWeekRow);
    QQuickControl::paddingChange(newPadding, oldPadding);
    if (isComponentComplete())
        d->resizeItems();
}

QT_END_NAMESPACE
