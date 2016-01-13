/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Layouts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquicklayout_p.h"
#include <QEvent>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qnumeric.h>
#include <limits>

/*!
    \qmltype Layout
    \instantiates QQuickLayoutAttached
    \inqmlmodule QtQuick.Layouts
    \ingroup layouts
    \brief Provides attached properties for items pushed onto a \l GridLayout,
    \l RowLayout or \l ColumnLayout.

    An object of type Layout is attached to children of the layout to provide layout specific
    information about the item.
    The properties of the attached object influences how the layout will arrange the items.

    For instance, you can specify \l minimumWidth, \l preferredWidth, and
    \l maximumWidth if the default values are not satisfactory.

    When a layout is resized, items may grow or shrink. Due to this, items have a
    \l{Layout::minimumWidth}{minimum size}, \l{Layout::preferredWidth}{preferred size} and a
    \l{Layout::maximumWidth}{maximum size}.

    If minimum size have not been explicitly specified on an item, the size is set to \c 0.
    If maximum size have not been explicitly specified on an item, the size is set to
    \c Number.POSITIVE_INFINITY.

    For layouts, the implicit minimum and maximum size depends on the content of the layouts.

    The \l fillWidth and \l fillHeight properties can either be \c true or \c false. If it is \c
    false, the item's size will be fixed to its preferred size. Otherwise, it will grow or shrink
    between its minimum and maximum size as the layout is resized.

    \note It is not recommended to have bindings to the x, y, width, or height properties of items
    in a layout, since this would conflict with the goal of the Layout, and also cause binding
    loops.


    \sa GridLayout
    \sa RowLayout
    \sa ColumnLayout
*/

QT_BEGIN_NAMESPACE

QQuickLayoutAttached::QQuickLayoutAttached(QObject *parent)
    : QObject(parent),
      m_minimumWidth(0),
      m_minimumHeight(0),
      m_preferredWidth(-1),
      m_preferredHeight(-1),
      m_maximumWidth(std::numeric_limits<qreal>::infinity()),
      m_maximumHeight(std::numeric_limits<qreal>::infinity()),
      m_row(-1),
      m_column(-1),
      m_rowSpan(1),
      m_columnSpan(1),
      m_fillWidth(false),
      m_fillHeight(false),
      m_isFillWidthSet(false),
      m_isFillHeightSet(false),
      m_isMinimumWidthSet(false),
      m_isMinimumHeightSet(false),
      m_isMaximumWidthSet(false),
      m_isMaximumHeightSet(false),
      m_changesNotificationEnabled(true),
      m_alignment(0)
{

}

/*!
    \qmlattachedproperty real Layout::minimumWidth

    This property holds the minimum width of an item in a layout.
    The default value is the items implicit minimum width.

    If the item is a layout, the implicit minimum width will be the minimum width the layout can
    have without any of its items shrink beyond their minimum width.
    The implicit minimum width for any other item is \c 0.

    Setting this value to -1 will reset the width back to its implicit minimum width.


    \sa preferredWidth
    \sa maximumWidth
*/
void QQuickLayoutAttached::setMinimumWidth(qreal width)
{
    if (qIsNaN(width))
        return;
    m_isMinimumWidthSet = width >= 0;
    if (m_minimumWidth == width)
        return;

    m_minimumWidth = width;
    invalidateItem();
    emit minimumWidthChanged();
}

/*!
    \qmlattachedproperty real Layout::minimumHeight

    The default value is the items implicit minimum height.

    If the item is a layout, the implicit minimum height will be the minimum height the layout can
    have without any of its items shrink beyond their minimum height.
    The implicit minimum height for any other item is \c 0.

    Setting this value to -1 will reset the height back to its implicit minimum height.

    \sa preferredHeight
    \sa maximumHeight
*/
void QQuickLayoutAttached::setMinimumHeight(qreal height)
{
    if (qIsNaN(height))
        return;
    m_isMinimumHeightSet = height >= 0;
    if (m_minimumHeight == height)
        return;

    m_minimumHeight = height;
    invalidateItem();
    emit minimumHeightChanged();
}

/*!
    \qmlattachedproperty real Layout::preferredWidth

    This property holds the preferred width of an item in a layout.
    If the preferred width is -1 it will be ignored, and the layout
    will use \l{Item::implicitWidth}{implicitWidth} instead.
    The default is \c -1.

    \sa minimumWidth
    \sa maximumWidth
*/
void QQuickLayoutAttached::setPreferredWidth(qreal width)
{
    if (qIsNaN(width) || m_preferredWidth == width)
        return;

    m_preferredWidth = width;
    invalidateItem();
    emit preferredWidthChanged();
}

/*!
    \qmlattachedproperty real Layout::preferredHeight

    This property holds the preferred height of an item in a layout.
    If the preferred height is -1 it will be ignored, and the layout
    will use \l{Item::implicitHeight}{implicitHeight} instead.
    The default is \c -1.

    \sa minimumHeight
    \sa maximumHeight
*/
void QQuickLayoutAttached::setPreferredHeight(qreal height)
{
    if (qIsNaN(height) || m_preferredHeight == height)
        return;

    m_preferredHeight = height;
    invalidateItem();
    emit preferredHeightChanged();
}

/*!
    \qmlattachedproperty real Layout::maximumWidth

    This property holds the maximum width of an item in a layout.
    The default value is the items implicit maximum width.

    If the item is a layout, the implicit maximum width will be the maximum width the layout can
    have without any of its items grow beyond their maximum width.
    The implicit maximum width for any other item is \c Number.POSITIVE_INFINITY.

    Setting this value to -1 will reset the width back to its implicit maximum width.

    \sa minimumWidth
    \sa preferredWidth
*/
void QQuickLayoutAttached::setMaximumWidth(qreal width)
{
    if (qIsNaN(width))
        return;
    m_isMaximumWidthSet = width >= 0;
    if (m_maximumWidth == width)
        return;

    m_maximumWidth = width;
    invalidateItem();
    emit maximumWidthChanged();
}

/*!
    \qmlattachedproperty real Layout::maximumHeight

    The default value is the items implicit maximum height.

    If the item is a layout, the implicit maximum height will be the maximum height the layout can
    have without any of its items grow beyond their maximum height.
    The implicit maximum height for any other item is \c Number.POSITIVE_INFINITY.

    Setting this value to -1 will reset the height back to its implicit maximum height.

    \sa minimumHeight
    \sa preferredHeight
*/
void QQuickLayoutAttached::setMaximumHeight(qreal height)
{
    if (qIsNaN(height))
        return;
    m_isMaximumHeightSet = height >= 0;
    if (m_maximumHeight == height)
        return;

    m_maximumHeight = height;
    invalidateItem();
    emit maximumHeightChanged();
}

void QQuickLayoutAttached::setMinimumImplicitSize(const QSizeF &sz)
{
    bool emitWidthChanged = false;
    bool emitHeightChanged = false;
    if (!m_isMinimumWidthSet && m_minimumWidth != sz.width()) {
        m_minimumWidth = sz.width();
        emitWidthChanged = true;
    }
    if (!m_isMinimumHeightSet && m_minimumHeight != sz.height()) {
        m_minimumHeight = sz.height();
        emitHeightChanged = true;
    }
    // Only invalidate the item once, and make sure we emit signal changed after the call to
    // invalidateItem()
    if (emitWidthChanged || emitHeightChanged) {
        invalidateItem();
        if (emitWidthChanged)
            emit minimumWidthChanged();
        if (emitHeightChanged)
            emit minimumHeightChanged();
    }
}

void QQuickLayoutAttached::setMaximumImplicitSize(const QSizeF &sz)
{
    bool emitWidthChanged = false;
    bool emitHeightChanged = false;
    if (!m_isMaximumWidthSet && m_maximumWidth != sz.width()) {
        m_maximumWidth = sz.width();
        emitWidthChanged = true;
    }
    if (!m_isMaximumHeightSet && m_maximumHeight != sz.height()) {
        m_maximumHeight = sz.height();
        emitHeightChanged = true;
    }
    // Only invalidate the item once, and make sure we emit changed signal after the call to
    // invalidateItem()
    if (emitWidthChanged || emitHeightChanged) {
        invalidateItem();
        if (emitWidthChanged)
            emit maximumWidthChanged();
        if (emitHeightChanged)
            emit maximumHeightChanged();
    }
}

/*!
    \qmlattachedproperty bool Layout::fillWidth

    If this property is \c true, the item will be as wide as possible while respecting
    the given constraints. If the property is \c false, the item will have a fixed width
    set to the preferred width.
    The default is \c false, except for layouts themselves, which default to \c true.

    \sa fillHeight
*/
void QQuickLayoutAttached::setFillWidth(bool fill)
{
    m_isFillWidthSet = true;
    if (m_fillWidth != fill) {
        m_fillWidth = fill;
        invalidateItem();
        emit fillWidthChanged();
    }
}

/*!
    \qmlattachedproperty bool Layout::fillHeight

    If this property is \c true, the item will be as tall as possible while respecting
    the given constraints. If the property is \c false, the item will have a fixed height
    set to the preferred height.
    The default is \c false, except for layouts themselves, which default to \c true.

    \sa fillWidth
*/
void QQuickLayoutAttached::setFillHeight(bool fill)
{
    m_isFillHeightSet = true;
    if (m_fillHeight != fill) {
        m_fillHeight = fill;
        invalidateItem();
        emit fillHeightChanged();
    }
}

/*!
    \qmlattachedproperty int Layout::row

    This property allows you to specify the row position of an item in a \l GridLayout.

    If both \l column and this property are not set, it is up to the layout to assign a cell to the item.

    The default value is \c 0.

    \sa column
    \sa rowSpan
*/
void QQuickLayoutAttached::setRow(int row)
{
    if (row >= 0 && row != m_row) {
        m_row = row;
        repopulateLayout();
        emit rowChanged();
    }
}

/*!
    \qmlattachedproperty int Layout::column

    This property allows you to specify the column position of an item in a \l GridLayout.

    If both \l row and this property are not set, it is up to the layout to assign a cell to the item.

    The default value is \c 0.

    \sa row
    \sa columnSpan
*/
void QQuickLayoutAttached::setColumn(int column)
{
    if (column >= 0 && column != m_column) {
        m_column = column;
        repopulateLayout();
        emit columnChanged();
    }
}


/*!
    \qmlattachedproperty Qt.Alignment Layout::alignment

    This property allows you to specify the alignment of an item within the cell(s) it occupies.

    The default value is \c 0, which means it will be \c{Qt.AlignVCenter | Qt.AlignLeft}

    A valid alignment is a combination of the following flags:
    \list
    \li Qt::AlignLeft
    \li Qt::AlignHCenter
    \li Qt::AlignRight
    \li Qt::AlignTop
    \li Qt::AlignVCenter
    \li Qt::AlignBottom
    \li Qt::AlignBaseline
    \endlist

*/
void QQuickLayoutAttached::setAlignment(Qt::Alignment align)
{
    if (align != m_alignment) {
        m_alignment = align;
        if (QQuickLayout *layout = parentLayout()) {
            layout->setAlignment(item(), align);
            invalidateItem();
        }
        emit alignmentChanged();
    }
}


/*!
    \qmlattachedproperty int Layout::rowSpan

    This property allows you to specify the row span of an item in a \l GridLayout.

    The default value is \c 1.

    \sa columnSpan
    \sa row
*/
void QQuickLayoutAttached::setRowSpan(int span)
{
    if (span != m_rowSpan) {
        m_rowSpan = span;
        repopulateLayout();
        emit rowSpanChanged();
    }
}


/*!
    \qmlattachedproperty int Layout::columnSpan

    This property allows you to specify the column span of an item in a \l GridLayout.

    The default value is \c 1.

    \sa rowSpan
    \sa column
*/
void QQuickLayoutAttached::setColumnSpan(int span)
{
    if (span != m_columnSpan) {
        m_columnSpan = span;
        repopulateLayout();
        emit columnSpanChanged();
    }
}


qreal QQuickLayoutAttached::sizeHint(Qt::SizeHint which, Qt::Orientation orientation) const
{
    qreal result = 0;
    if (QQuickLayout *layout = qobject_cast<QQuickLayout *>(item())) {
        const QSizeF sz = layout->sizeHint(which);
        result = (orientation == Qt::Horizontal ? sz.width() : sz.height());
    } else {
        if (which == Qt::MaximumSize)
            result = std::numeric_limits<qreal>::infinity();
    }
    return result;
}

void QQuickLayoutAttached::invalidateItem()
{
    if (!m_changesNotificationEnabled)
        return;
    quickLayoutDebug() << "QQuickLayoutAttached::invalidateItem";
    if (QQuickLayout *layout = parentLayout()) {
        layout->invalidate(item());
    }
}

void QQuickLayoutAttached::repopulateLayout()
{
    if (QQuickLayout *layout = parentLayout())
        layout->updateLayoutItems();
}

QQuickLayout *QQuickLayoutAttached::parentLayout() const
{
    QQuickItem *parentItem = item();
    if (parentItem) {
        parentItem = parentItem->parentItem();
        return qobject_cast<QQuickLayout *>(parentItem);
    } else {
        qWarning("Layout must be attached to Item elements");
    }
    return 0;
}

QQuickItem *QQuickLayoutAttached::item() const
{
    return qobject_cast<QQuickItem *>(parent());
}





QQuickLayout::QQuickLayout(QQuickLayoutPrivate &dd, QQuickItem *parent)
    : QQuickItem(dd, parent),
      m_dirty(false)
{
}

QQuickLayout::~QQuickLayout()
{

}

QQuickLayoutAttached *QQuickLayout::qmlAttachedProperties(QObject *object)
{
    return new QQuickLayoutAttached(object);
}

void QQuickLayout::updatePolish()
{
    rearrange(QSizeF(width(), height()));
}

void QQuickLayout::componentComplete()
{
    QQuickItem::componentComplete();
}

void QQuickLayout::invalidate(QQuickItem * /*childItem*/)
{
    if (m_dirty)
        return;

    m_dirty = true;

    if (!qobject_cast<QQuickLayout *>(parentItem())) {
        quickLayoutDebug() << "QQuickLayout::invalidate(), polish()";
        polish();
    }
}

void QQuickLayout::rearrange(const QSizeF &/*size*/)
{
    m_dirty = false;
}

QT_END_NAMESPACE
