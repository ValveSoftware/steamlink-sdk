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

#include "qquickweeknumbercolumn_p.h"
#include "qquickweeknumbermodel_p.h"

#include <QtQuickTemplates2/private/qquickcontrol_p_p.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype WeekNumberColumn
    \inherits Control
    \instantiates QQuickWeekNumberColumn
    \inqmlmodule Qt.labs.calendar
    \brief A column of week numbers.

    WeekNumberColumn presents week numbers in a column. The week numbers
    are calculated for a given \l month and \l year, using the specified
    \l {Control::locale}{locale}.

    \image qtlabscalendar-weeknumbercolumn.png
    \snippet qtlabscalendar-weeknumbercolumn.qml 1

    WeekNumberColumn can be used as a standalone control, but it is most
    often used in conjunction with MonthGrid. Regardless of the use case,
    positioning of the column is left to the user.

    \image qtlabscalendar-weeknumbercolumn-layout.png
    \snippet qtlabscalendar-weeknumbercolumn-layout.qml 1

    The visual appearance of WeekNumberColumn can be changed by
    implementing a \l {delegate}{custom delegate}.

    \labs

    \sa MonthGrid, DayOfWeekRow
*/

class QQuickWeekNumberColumnPrivate : public QQuickControlPrivate
{
public:
    QQuickWeekNumberColumnPrivate() : delegate(nullptr), model(nullptr) { }

    void resizeItems();

    QVariant source;
    QQmlComponent *delegate;
    QQuickWeekNumberModel *model;
};

void QQuickWeekNumberColumnPrivate::resizeItems()
{
    if (!contentItem)
        return;

    QSizeF itemSize;
    itemSize.setWidth(contentItem->width());
    itemSize.setHeight((contentItem->height() - 5 * spacing) / 6);

    const auto childItems = contentItem->childItems();
    for (QQuickItem *item : childItems)
        item->setSize(itemSize);
}

QQuickWeekNumberColumn::QQuickWeekNumberColumn(QQuickItem *parent) :
    QQuickControl(*(new QQuickWeekNumberColumnPrivate), parent)
{
    Q_D(QQuickWeekNumberColumn);
    d->model = new QQuickWeekNumberModel(this);
    d->source = QVariant::fromValue(d->model);
    connect(d->model, &QQuickWeekNumberModel::monthChanged, this, &QQuickWeekNumberColumn::monthChanged);
    connect(d->model, &QQuickWeekNumberModel::yearChanged, this, &QQuickWeekNumberColumn::yearChanged);
}

/*!
    \qmlproperty int Qt.labs.calendar::WeekNumberColumn::month

    This property holds the number of the month that the week numbers are
    calculated for. The default value is the current month.

    The Qt Labs Calendar module uses 0-based month numbers to be consistent
    with the JavaScript Date type, that is used by the QML language. This
    means that \c Date::getMonth() can be assigned to this property as is.
    When dealing with dealing with month numbers directly, it is highly
    recommended to use the following enumeration values to avoid confusion.

    \value Calendar.January January (0)
    \value Calendar.February February (1)
    \value Calendar.March March (2)
    \value Calendar.April April (3)
    \value Calendar.May May (4)
    \value Calendar.June June (5)
    \value Calendar.July July (6)
    \value Calendar.August August (7)
    \value Calendar.September September (8)
    \value Calendar.October October (9)
    \value Calendar.November November (10)
    \value Calendar.December December (11)

    \sa Calendar
*/
int QQuickWeekNumberColumn::month() const
{
    Q_D(const QQuickWeekNumberColumn);
    return d->model->month() - 1;
}

void QQuickWeekNumberColumn::setMonth(int month)
{
    Q_D(QQuickWeekNumberColumn);
    if (month < 0 || month > 11) {
        qmlInfo(this) << "month " << month << " is out of range [0...11]";
        return;
    }
    d->model->setMonth(month + 1);
}

/*!
    \qmlproperty int Qt.labs.calendar::WeekNumberColumn::year

    This property holds the number of the year that the week numbers are calculated for.

    The value must be in the range from \c -271820 to \c 275759. The default
    value is the current year.
*/
int QQuickWeekNumberColumn::year() const
{
    Q_D(const QQuickWeekNumberColumn);
    return d->model->year();
}

void QQuickWeekNumberColumn::setYear(int year)
{
    Q_D(QQuickWeekNumberColumn);
    if (year < -271820 || year > 275759) {
        qmlInfo(this) << "year " << year << " is out of range [-271820...275759]";
        return;
    }
    d->model->setYear(year);
}

/*!
    \internal
    \qmlproperty model Qt.labs.calendar::WeekNumberColumn::source

    This property holds the source model that is used as a data model
    for the internal content column.
*/
QVariant QQuickWeekNumberColumn::source() const
{
    Q_D(const QQuickWeekNumberColumn);
    return d->source;
}

void QQuickWeekNumberColumn::setSource(const QVariant &source)
{
    Q_D(QQuickWeekNumberColumn);
    if (d->source != source) {
        d->source = source;
        emit sourceChanged();
    }
}

/*!
    \qmlproperty Component Qt.labs.calendar::WeekNumberColumn::delegate

    This property holds the item delegate that visualizes each week number.

    In addition to the \c index property, a list of model data roles
    are available in the context of each delegate:
    \table
        \row \li \b model.weekNumber : int \li The week number
    \endtable

    The following snippet presents the default implementation of the item
    delegate. It can be used as a starting point for implementing custom
    delegates.

    \snippet WeekNumberColumn.qml delegate
*/
QQmlComponent *QQuickWeekNumberColumn::delegate() const
{
    Q_D(const QQuickWeekNumberColumn);
    return d->delegate;
}

void QQuickWeekNumberColumn::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickWeekNumberColumn);
    if (d->delegate != delegate) {
        d->delegate = delegate;
        emit delegateChanged();
    }
}

void QQuickWeekNumberColumn::componentComplete()
{
    Q_D(QQuickWeekNumberColumn);
    QQuickControl::componentComplete();
    d->resizeItems();
}

void QQuickWeekNumberColumn::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickWeekNumberColumn);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    if (isComponentComplete())
        d->resizeItems();
}

void QQuickWeekNumberColumn::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_D(QQuickWeekNumberColumn);
    QQuickControl::localeChange(newLocale, oldLocale);
    d->model->setLocale(newLocale);
}

void QQuickWeekNumberColumn::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickWeekNumberColumn);
    QQuickControl::paddingChange(newPadding, oldPadding);
    if (isComponentComplete())
        d->resizeItems();
}

QT_END_NAMESPACE
