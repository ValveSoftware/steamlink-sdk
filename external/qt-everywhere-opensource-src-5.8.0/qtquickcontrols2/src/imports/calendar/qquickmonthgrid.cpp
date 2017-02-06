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

#include "qquickmonthgrid_p.h"
#include "qquickmonthmodel_p.h"

#include <QtGui/qstylehints.h>
#include <QtGui/qguiapplication.h>
#include <QtQuickTemplates2/private/qquickcontrol_p_p.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype MonthGrid
    \inherits Control
    \instantiates QQuickMonthGrid
    \inqmlmodule Qt.labs.calendar
    \brief A grid of days for a calendar month.

    MonthGrid presents a calendar month in a grid. The contents are
    calculated for a given \l month and \l year, using the specified
    \l {Control::locale}{locale}.

    \image qtlabscalendar-monthgrid.png
    \snippet qtlabscalendar-monthgrid.qml 1

    MonthGrid can be used as a standalone control, but it is most often
    used in conjunction with DayOfWeekRow and WeekNumberColumn. Regardless
    of the use case, positioning of the grid is left to the user.

    \image qtlabscalendar-monthgrid-layout.png
    \snippet qtlabscalendar-monthgrid-layout.qml 1

    The visual appearance of MonthGrid can be changed by
    implementing a \l {delegate}{custom delegate}.

    \labs

    \sa DayOfWeekRow, WeekNumberColumn, CalendarModel
*/

/*!
    \qmlsignal Qt.labs.calendar::MonthGrid::pressed(date date)

    This signal is emitted when \a date is pressed.
*/

/*!
    \qmlsignal Qt.labs.calendar::MonthGrid::released(date date)

    This signal is emitted when \a date is released.
*/

/*!
    \qmlsignal Qt.labs.calendar::MonthGrid::clicked(date date)

    This signal is emitted when \a date is clicked.
*/

/*!
    \qmlsignal Qt.labs.calendar::MonthGrid::pressAndHold(date date)

    This signal is emitted when \a date is pressed and held down.
*/

class QQuickMonthGridPrivate : public QQuickControlPrivate
{
    Q_DECLARE_PUBLIC(QQuickMonthGrid)

public:
    QQuickMonthGridPrivate() : pressTimer(0), pressedItem(nullptr), model(nullptr), delegate(nullptr) { }

    void resizeItems();

    QQuickItem *cellAt(const QPoint &pos) const;
    QDate dateOf(QQuickItem *cell) const;

    void updatePress(const QPoint &pos);
    void clearPress(bool clicked);

    static void setContextProperty(QQuickItem *item, const QString &name, const QVariant &value);

    QString title;
    QVariant source;
    QDate pressedDate;
    int pressTimer;
    QQuickItem *pressedItem;
    QQuickMonthModel *model;
    QQmlComponent *delegate;
};

void QQuickMonthGridPrivate::resizeItems()
{
    if (!contentItem)
        return;

    QSizeF itemSize;
    itemSize.setWidth((contentItem->width() - 6 * spacing) / 7);
    itemSize.setHeight((contentItem->height() - 5 * spacing) / 6);

    const auto childItems = contentItem->childItems();
    for (QQuickItem *item : childItems)
        item->setSize(itemSize);
}

QQuickItem *QQuickMonthGridPrivate::cellAt(const QPoint &pos) const
{
    Q_Q(const QQuickMonthGrid);
    if (contentItem) {
        QPointF mapped = q->mapToItem(contentItem, pos);
        return contentItem->childAt(mapped.x(), mapped.y());
    }
    return nullptr;
}

QDate QQuickMonthGridPrivate::dateOf(QQuickItem *cell) const
{
    if (contentItem)
        return model->dateAt(contentItem->childItems().indexOf(cell));
    return QDate();
}

void QQuickMonthGridPrivate::updatePress(const QPoint &pos)
{
    Q_Q(QQuickMonthGrid);
    clearPress(false);
    pressedItem = cellAt(pos);
    setContextProperty(pressedItem, QStringLiteral("pressed"), true);
    pressedDate = dateOf(pressedItem);
    if (pressedDate.isValid())
        emit q->pressed(pressedDate);
}

void QQuickMonthGridPrivate::clearPress(bool clicked)
{
    Q_Q(QQuickMonthGrid);
    setContextProperty(pressedItem, QStringLiteral("pressed"), false);
    if (pressedDate.isValid()) {
        emit q->released(pressedDate);
        if (clicked)
            emit q->clicked(pressedDate);
    }
    pressedDate = QDate();
    pressedItem = nullptr;
}

void QQuickMonthGridPrivate::setContextProperty(QQuickItem *item, const QString &name, const QVariant &value)
{
    QQmlContext *context = qmlContext(item);
    if (context && context->isValid()) {
        context = context->parentContext();
        if (context && context->isValid())
            context->setContextProperty(name, value);
    }
}

QQuickMonthGrid::QQuickMonthGrid(QQuickItem *parent) :
    QQuickControl(*(new QQuickMonthGridPrivate), parent)
{
    Q_D(QQuickMonthGrid);
    setFlag(ItemIsFocusScope);
    setActiveFocusOnTab(true);
    setAcceptedMouseButtons(Qt::LeftButton);

    d->model = new QQuickMonthModel(this);
    d->source = QVariant::fromValue(d->model);
    connect(d->model, &QQuickMonthModel::monthChanged, this, &QQuickMonthGrid::monthChanged);
    connect(d->model, &QQuickMonthModel::yearChanged, this, &QQuickMonthGrid::yearChanged);
    connect(d->model, &QQuickMonthModel::titleChanged, this, &QQuickMonthGrid::titleChanged);
}

/*!
    \qmlproperty int Qt.labs.calendar::MonthGrid::month

    This property holds the number of the month. The default value is the
    current month.

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
int QQuickMonthGrid::month() const
{
    Q_D(const QQuickMonthGrid);
    return d->model->month() - 1;
}

void QQuickMonthGrid::setMonth(int month)
{
    Q_D(QQuickMonthGrid);
    if (month < 0 || month > 11) {
        qmlInfo(this) << "month " << month << " is out of range [0...11]";
        return;
    }
    d->model->setMonth(month + 1);
}

/*!
    \qmlproperty int Qt.labs.calendar::MonthGrid::year

    This property holds the number of the year.

    The value must be in the range from \c -271820 to \c 275759. The default
    value is the current year.
*/
int QQuickMonthGrid::year() const
{
    Q_D(const QQuickMonthGrid);
    return d->model->year();
}

void QQuickMonthGrid::setYear(int year)
{
    Q_D(QQuickMonthGrid);
    if (year < -271820 || year > 275759) {
        qmlInfo(this) << "year " << year << " is out of range [-271820...275759]";
        return;
    }
    d->model->setYear(year);
}

/*!
    \internal
    \qmlproperty model Qt.labs.calendar::MonthGrid::source

    This property holds the source model that is used as a data model
    for the internal content column.
*/
QVariant QQuickMonthGrid::source() const
{
    Q_D(const QQuickMonthGrid);
    return d->source;
}

void QQuickMonthGrid::setSource(const QVariant &source)
{
    Q_D(QQuickMonthGrid);
    if (d->source != source) {
        d->source = source;
        emit sourceChanged();
    }
}

/*!
    \qmlproperty string Qt.labs.calendar::MonthGrid::title

    This property holds a title for the calendar.

    This property is provided for convenience. MonthGrid itself does
    not visualize the title. The default value consists of the month name,
    formatted using \l {Control::locale}{locale}, and the year number.
*/
QString QQuickMonthGrid::title() const
{
    Q_D(const QQuickMonthGrid);
    if (d->title.isNull())
        return d->model->title();
    return d->title;
}

void QQuickMonthGrid::setTitle(const QString &title)
{
    Q_D(QQuickMonthGrid);
    if (d->title != title) {
        d->title = title;
        emit titleChanged();
    }
}

/*!
    \qmlproperty Component Qt.labs.calendar::MonthGrid::delegate

    This property holds the item delegate that visualizes each day.

    In addition to the \c index property, a list of model data roles
    are available in the context of each delegate:
    \table
        \row \li \b model.date : date \li The date of the cell
        \row \li \b model.day : int \li The number of the day
        \row \li \b model.today : bool \li Whether the delegate represents today
        \row \li \b model.weekNumber : int \li The week number
        \row \li \b model.month : int \li The number of the month
        \row \li \b model.year : int \li The number of the year
    \endtable

    The following snippet presents the default implementation of the item
    delegate. It can be used as a starting point for implementing custom
    delegates.

    \snippet MonthGrid.qml delegate
*/
QQmlComponent *QQuickMonthGrid::delegate() const
{
    Q_D(const QQuickMonthGrid);
    return d->delegate;
}

void QQuickMonthGrid::setDelegate(QQmlComponent *delegate)
{
    Q_D(QQuickMonthGrid);
    if (d->delegate != delegate) {
        d->delegate = delegate;
        emit delegateChanged();
    }
}

void QQuickMonthGrid::componentComplete()
{
    Q_D(QQuickMonthGrid);
    QQuickControl::componentComplete();
    if (d->contentItem) {
        const auto childItems = d->contentItem->childItems();
        for (QQuickItem *child : childItems) {
            if (!QQuickItemPrivate::get(child)->isTransparentForPositioner())
                d->setContextProperty(child, QStringLiteral("pressed"), false);
        }
    }
    d->resizeItems();
}

void QQuickMonthGrid::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickMonthGrid);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);
    if (isComponentComplete())
        d->resizeItems();
}

void QQuickMonthGrid::localeChange(const QLocale &newLocale, const QLocale &oldLocale)
{
    Q_D(QQuickMonthGrid);
    QQuickControl::localeChange(newLocale, oldLocale);
    d->model->setLocale(newLocale);
}

void QQuickMonthGrid::paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding)
{
    Q_D(QQuickMonthGrid);
    QQuickControl::paddingChange(newPadding, oldPadding);
    if (isComponentComplete())
        d->resizeItems();
}

void QQuickMonthGrid::updatePolish()
{
    Q_D(QQuickMonthGrid);
    QQuickControl::updatePolish();
    d->resizeItems();
}

void QQuickMonthGrid::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickMonthGrid);
    d->updatePress(event->pos());
    if (d->pressedDate.isValid())
        d->pressTimer = startTimer(qGuiApp->styleHints()->mousePressAndHoldInterval());
    event->accept();
}

void QQuickMonthGrid::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickMonthGrid);
    d->updatePress(event->pos());
    event->accept();
}

void QQuickMonthGrid::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickMonthGrid);
    d->clearPress(true);
    event->accept();
}

void QQuickMonthGrid::mouseUngrabEvent()
{
    Q_D(QQuickMonthGrid);
    d->clearPress(false);
}

void QQuickMonthGrid::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickMonthGrid);
    if (event->timerId() == d->pressTimer) {
        if (d->pressedDate.isValid())
            emit pressAndHold(d->pressedDate);
        killTimer(d->pressTimer);
    }
}

QT_END_NAMESPACE
