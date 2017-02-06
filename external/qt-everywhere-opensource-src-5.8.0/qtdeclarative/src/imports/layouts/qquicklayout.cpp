/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Layouts module of the Qt Toolkit.
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

#include "qquicklayout_p.h"
#include <QEvent>
#include <QtCore/qcoreapplication.h>
#include <QtCore/private/qnumeric_p.h>
#include <QtCore/qmath.h>
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
    The properties of the attached object influence how the layout will arrange the items.

    For instance, you can specify \l minimumWidth, \l preferredWidth, and
    \l maximumWidth if the default values are not satisfactory.

    When a layout is resized, items may grow or shrink. Due to this, items have a
    \l{Layout::minimumWidth}{minimum size}, \l{Layout::preferredWidth}{preferred size} and a
    \l{Layout::maximumWidth}{maximum size}.

    If minimum size has not been explicitly specified on an item, the size is set to \c 0.
    If maximum size has not been explicitly specified on an item, the size is set to
    \c Number.POSITIVE_INFINITY.

    For layouts, the implicit minimum and maximum sizes depend on the content of the layouts.

    The \l fillWidth and \l fillHeight properties can either be \c true or \c false. If they are \c
    false, the item's size will be fixed to its preferred size. Otherwise, it will grow or shrink
    between its minimum and maximum size as the layout is resized.

    \note It is not recommended to have bindings to the x, y, width, or height properties of items
    in a layout, since this would conflict with the goals of Layout, and can also cause binding
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
      m_defaultMargins(0),
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
      m_isLeftMarginSet(false),
      m_isTopMarginSet(false),
      m_isRightMarginSet(false),
      m_isBottomMarginSet(false),
      m_alignment(0)
{

}

/*!
    \qmlattachedproperty real Layout::minimumWidth

    This property holds the minimum width of an item in a layout.
    The default value is the item's implicit minimum width.

    If the item is a layout, the implicit minimum width will be the minimum width the layout can
    have without any of its items shrinking below their minimum width.
    The implicit minimum width for any other item is \c 0.

    Setting this value to -1 will reset the width back to its implicit minimum width.


    \sa preferredWidth
    \sa maximumWidth
*/
void QQuickLayoutAttached::setMinimumWidth(qreal width)
{
    if (qt_is_nan(width))
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

    This property holds the minimum height of an item in a layout.
    The default value is the item's implicit minimum height.

    If the item is a layout, the implicit minimum height will be the minimum height the layout can
    have without any of its items shrinking below their minimum height.
    The implicit minimum height for any other item is \c 0.

    Setting this value to -1 will reset the height back to its implicit minimum height.

    \sa preferredHeight
    \sa maximumHeight
*/
void QQuickLayoutAttached::setMinimumHeight(qreal height)
{
    if (qt_is_nan(height))
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
    If the preferred width is \c -1 it will be ignored, and the layout
    will use \l{Item::implicitWidth}{implicitWidth} instead.
    The default is \c -1.

    \sa minimumWidth
    \sa maximumWidth
*/
void QQuickLayoutAttached::setPreferredWidth(qreal width)
{
    if (qt_is_nan(width) || m_preferredWidth == width)
        return;

    m_preferredWidth = width;
    invalidateItem();
    emit preferredWidthChanged();
}

/*!
    \qmlattachedproperty real Layout::preferredHeight

    This property holds the preferred height of an item in a layout.
    If the preferred height is \c -1 it will be ignored, and the layout
    will use \l{Item::implicitHeight}{implicitHeight} instead.
    The default is \c -1.

    \sa minimumHeight
    \sa maximumHeight
*/
void QQuickLayoutAttached::setPreferredHeight(qreal height)
{
    if (qt_is_nan(height) || m_preferredHeight == height)
        return;

    m_preferredHeight = height;
    invalidateItem();
    emit preferredHeightChanged();
}

/*!
    \qmlattachedproperty real Layout::maximumWidth

    This property holds the maximum width of an item in a layout.
    The default value is the item's implicit maximum width.

    If the item is a layout, the implicit maximum width will be the maximum width the layout can
    have without any of its items growing beyond their maximum width.
    The implicit maximum width for any other item is \c Number.POSITIVE_INFINITY.

    Setting this value to \c -1 will reset the width back to its implicit maximum width.

    \sa minimumWidth
    \sa preferredWidth
*/
void QQuickLayoutAttached::setMaximumWidth(qreal width)
{
    if (qt_is_nan(width))
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

    The default value is the item's implicit maximum height.

    If the item is a layout, the implicit maximum height will be the maximum height the layout can
    have without any of its items growing beyond their maximum height.
    The implicit maximum height for any other item is \c Number.POSITIVE_INFINITY.

    Setting this value to \c -1 will reset the height back to its implicit maximum height.

    \sa minimumHeight
    \sa preferredHeight
*/
void QQuickLayoutAttached::setMaximumHeight(qreal height)
{
    if (qt_is_nan(height))
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
    \qmlattachedproperty real Layout::margins

    Sets the margins outside of an item to all have the same value. The item
    itself does not evaluate its own margins. It is the parent's responsibility
    to decide if it wants to evaluate the margins.

    Specifically, margins are only evaluated by ColumnLayout, RowLayout,
    GridLayout, and other layout-like containers, such as SplitView, where the
    effective cell size of an item will be increased as the margins are
    increased.

    Therefore, if an item with margins is a child of another \c Item, its
    position, size and implicit size will remain unchanged.

    Combining margins with alignment will align the item \e including its
    margins. For instance, a vertically-centered Item with a top margin of \c 1
    and a bottom margin of \c 9 will cause the Items effective alignment within
    the cell to be 4 pixels above the center.

    The default value is \c 0.

    \sa leftMargin
    \sa topMargin
    \sa rightMargin
    \sa bottomMargin

    \since QtQuick.Layouts 1.2
*/
void QQuickLayoutAttached::setMargins(qreal m)
{
    if (m == m_defaultMargins)
        return;

    m_defaultMargins = m;
    invalidateItem();
    if (!m_isLeftMarginSet && m_margins.left() != m)
        emit leftMarginChanged();
    if (!m_isTopMarginSet && m_margins.top() != m)
        emit topMarginChanged();
    if (!m_isRightMarginSet && m_margins.right() != m)
        emit rightMarginChanged();
    if (!m_isBottomMarginSet && m_margins.bottom() != m)
        emit bottomMarginChanged();
    emit marginsChanged();
}

/*!
    \qmlattachedproperty real Layout::leftMargin

    Specifies the left margin outside of an item.
    If the value is not set, it will use the value from \l margins.

    \sa margins

    \since QtQuick.Layouts 1.2
*/
void QQuickLayoutAttached::setLeftMargin(qreal m)
{
    const bool changed = leftMargin() != m;
    m_margins.setLeft(m);
    m_isLeftMarginSet = true;
    if (changed) {
        invalidateItem();
        emit leftMarginChanged();
    }
}

void QQuickLayoutAttached::resetLeftMargin()
{
    const bool changed = m_isLeftMarginSet && (m_defaultMargins != m_margins.left());
    m_isLeftMarginSet = false;
    if (changed) {
        invalidateItem();
        emit leftMarginChanged();
    }
}

/*!
    \qmlattachedproperty real Layout::topMargin

    Specifies the top margin outside of an item.
    If the value is not set, it will use the value from \l margins.

    \sa margins

    \since QtQuick.Layouts 1.2
*/
void QQuickLayoutAttached::setTopMargin(qreal m)
{
    const bool changed = topMargin() != m;
    m_margins.setTop(m);
    m_isTopMarginSet = true;
    if (changed) {
        invalidateItem();
        emit topMarginChanged();
    }
}

void QQuickLayoutAttached::resetTopMargin()
{
    const bool changed = m_isTopMarginSet && (m_defaultMargins != m_margins.top());
    m_isTopMarginSet = false;
    if (changed) {
        invalidateItem();
        emit topMarginChanged();
    }
}

/*!
    \qmlattachedproperty real Layout::rightMargin

    Specifies the right margin outside of an item.
    If the value is not set, it will use the value from \l margins.

    \sa margins

    \since QtQuick.Layouts 1.2
*/
void QQuickLayoutAttached::setRightMargin(qreal m)
{
    const bool changed = rightMargin() != m;
    m_margins.setRight(m);
    m_isRightMarginSet = true;
    if (changed) {
        invalidateItem();
        emit rightMarginChanged();
    }
}

void QQuickLayoutAttached::resetRightMargin()
{
    const bool changed = m_isRightMarginSet && (m_defaultMargins != m_margins.right());
    m_isRightMarginSet = false;
    if (changed) {
        invalidateItem();
        emit rightMarginChanged();
    }
}

/*!
    \qmlattachedproperty real Layout::bottomMargin

    Specifies the bottom margin outside of an item.
    If the value is not set, it will use the value from \l margins.

    \sa margins

    \since QtQuick.Layouts 1.2
*/
void QQuickLayoutAttached::setBottomMargin(qreal m)
{
    const bool changed = bottomMargin() != m;
    m_margins.setBottom(m);
    m_isBottomMarginSet = true;
    if (changed) {
        invalidateItem();
        emit bottomMarginChanged();
    }
}

void QQuickLayoutAttached::resetBottomMargin()
{
    const bool changed = m_isBottomMarginSet && (m_defaultMargins != m_margins.bottom());
    m_isBottomMarginSet = false;
    if (changed) {
        invalidateItem();
        emit bottomMarginChanged();
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
    d_func()->m_isReady = false;

    const auto childItems = d_func()->childItems;
    for (QQuickItem *child : childItems)
        QQuickItemPrivate::get(child)->removeItemChangeListener(this, QQuickItemPrivate::SiblingOrder);
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
    Q_D(QQuickLayout);
    d->m_disableRearrange = true;
    QQuickItem::componentComplete();    // will call our geometryChanged(), (where isComponentComplete() == true)
    d->m_disableRearrange = false;
    d->m_isReady = true;
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

bool QQuickLayout::shouldIgnoreItem(QQuickItem *child, QQuickLayoutAttached *&info, QSizeF *sizeHints) const
{
    Q_D(const QQuickLayout);
    bool ignoreItem = true;
    QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);
    if (childPrivate->explicitVisible) {
        effectiveSizeHints_helper(child, sizeHints, &info, true);
        QSizeF effectiveMaxSize = sizeHints[Qt::MaximumSize];
        if (!effectiveMaxSize.isNull()) {
            QSizeF &prefS = sizeHints[Qt::PreferredSize];
            if (effectiveSizePolicy_helper(child, Qt::Horizontal, info) == QLayoutPolicy::Fixed)
                effectiveMaxSize.setWidth(prefS.width());
            if (effectiveSizePolicy_helper(child, Qt::Vertical, info) == QLayoutPolicy::Fixed)
                effectiveMaxSize.setHeight(prefS.height());
        }
        ignoreItem = effectiveMaxSize.isNull();
    }

    if (ignoreItem)
        d->m_ignoredItems << child;
    return ignoreItem;
}

void QQuickLayout::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemChildAddedChange) {
        QQuickItem *item = value.item;
        qmlobject_connect(item, QQuickItem, SIGNAL(baselineOffsetChanged(qreal)), this, QQuickLayout, SLOT(invalidateSenderItem()));
        QQuickItemPrivate::get(item)->addItemChangeListener(this, QQuickItemPrivate::SiblingOrder | QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight | QQuickItemPrivate::Destroyed | QQuickItemPrivate::Visibility);
        if (isReady())
            updateLayoutItems();
    } else if (change == ItemChildRemovedChange) {
        QQuickItem *item = value.item;
        qmlobject_disconnect(item, QQuickItem, SIGNAL(baselineOffsetChanged(qreal)), this, QQuickLayout, SLOT(invalidateSenderItem()));
        QQuickItemPrivate::get(item)->removeItemChangeListener(this, QQuickItemPrivate::SiblingOrder | QQuickItemPrivate::ImplicitWidth | QQuickItemPrivate::ImplicitHeight | QQuickItemPrivate::Destroyed | QQuickItemPrivate::Visibility);
        if (isReady())
            updateLayoutItems();
    }
    QQuickItem::itemChange(change, value);
}

void QQuickLayout::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickLayout);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (d->m_disableRearrange || !isReady() || !newGeometry.isValid())
        return;

    quickLayoutDebug() << "QQuickStackLayout::geometryChanged" << newGeometry << oldGeometry;
    rearrange(newGeometry.size());
}

void QQuickLayout::invalidateSenderItem()
{
    if (!isReady())
        return;
    QQuickItem *item = static_cast<QQuickItem *>(sender());
    Q_ASSERT(item);
    invalidate(item);
}

bool QQuickLayout::isReady() const
{
    return d_func()->m_isReady;
}

void QQuickLayout::itemSiblingOrderChanged(QQuickItem *item)
{
    Q_UNUSED(item);
    updateLayoutItems();
}

void QQuickLayout::itemImplicitWidthChanged(QQuickItem *item)
{
    if (!isReady() || item->signalsBlocked())
        return;
    invalidate(item);
}

void QQuickLayout::itemImplicitHeightChanged(QQuickItem *item)
{
    if (!isReady() || item->signalsBlocked())
        return;
    invalidate(item);
}

void QQuickLayout::itemDestroyed(QQuickItem *item)
{
    Q_UNUSED(item);
}

void QQuickLayout::itemVisibilityChanged(QQuickItem *item)
{
    Q_UNUSED(item);
}

void QQuickLayout::rearrange(const QSizeF &/*size*/)
{
    m_dirty = false;
}


/*
  The layout engine assumes:
    1. minimum <= preferred <= maximum
    2. descent is within minimum and maximum bounds     (### verify)

    This function helps to ensure that by the following rules (in the following order):
    1. If minimum > maximum, set minimum = maximum
    2. Clamp preferred to be between the [minimum,maximum] range.
    3. If descent > minimum, set descent = minimum      (### verify if this is correct, it might
                                                        need some refinements to multiline texts)

    If any values are "not set" (i.e. negative), they will be left untouched, so that we
    know which values needs to be fetched from the implicit hints (not user hints).
  */
static void normalizeHints(qreal &minimum, qreal &preferred, qreal &maximum, qreal &descent)
{
    if (minimum >= 0 && maximum >= 0 && minimum > maximum)
        minimum = maximum;

    if (preferred >= 0) {
        if (minimum >= 0 && preferred < minimum) {
            preferred = minimum;
        } else if (maximum >= 0 && preferred > maximum) {
            preferred = maximum;
        }
    }

    if (minimum >= 0 && descent > minimum)
        descent = minimum;
}

static void boundSize(QSizeF &result, const QSizeF &size)
{
    if (size.width() >= 0 && size.width() < result.width())
        result.setWidth(size.width());
    if (size.height() >= 0 && size.height() < result.height())
        result.setHeight(size.height());
}

static void expandSize(QSizeF &result, const QSizeF &size)
{
    if (size.width() >= 0 && size.width() > result.width())
        result.setWidth(size.width());
    if (size.height() >= 0 && size.height() > result.height())
        result.setHeight(size.height());
}

static inline void combineHints(qreal &current, qreal fallbackHint)
{
    if (current < 0)
        current = fallbackHint;
}

static inline void combineSize(QSizeF &result, const QSizeF &fallbackSize)
{
    combineHints(result.rwidth(), fallbackSize.width());
    combineHints(result.rheight(), fallbackSize.height());
}

static inline void combineImplicitHints(QQuickLayoutAttached *info, Qt::SizeHint which, QSizeF *size)
{
    if (!info) return;

    Q_ASSERT(which == Qt::MinimumSize || which == Qt::MaximumSize);

    const QSizeF constraint(which == Qt::MinimumSize
                            ? QSizeF(info->minimumWidth(), info->minimumHeight())
                            : QSizeF(info->maximumWidth(), info->maximumHeight()));

    if (!info->isExtentExplicitlySet(Qt::Horizontal, which))
        combineHints(size->rwidth(),  constraint.width());
    if (!info->isExtentExplicitlySet(Qt::Vertical, which))
        combineHints(size->rheight(), constraint.height());
}

typedef qreal (QQuickLayoutAttached::*SizeGetter)() const;

/*!
    \internal
    Note: Can potentially return the attached QQuickLayoutAttached object through \a attachedInfo.

    It is like this is because it enables it to be reused.

    The goal of this function is to return the effective minimum, preferred and maximum size hints
    that the layout will use for this item.
    This function takes care of gathering all explicitly set size hints, normalizes them so
    that min < pref < max.
    Further, the hints _not_explicitly_ set will then be initialized with the implicit size hints,
    which is usually derived from the content of the layouts (or items).

    The following table illustrates the preference of the properties used for measuring layout
    items. If present, the USER properties will be preferred. If USER properties are not present,
    the HINT properties will be preferred. Finally, the FALLBACK properties will be used as an
    ultimate fallback.

    Note that one can query if the value of Layout.minimumWidth or Layout.maximumWidth has been
    explicitly or implicitly set with QQuickLayoutAttached::isExtentExplicitlySet(). This
    determines if it should be used as a USER or as a HINT value.

    Fractional size hints will be ceiled to the closest integer. This is in order to give some
    slack when the items are snapped to the pixel grid.

                 | *Minimum*            | *Preferred*           | *Maximum*                |
+----------------+----------------------+-----------------------+--------------------------+
|USER (explicit) | Layout.minimumWidth  | Layout.preferredWidth | Layout.maximumWidth      |
|HINT (implicit) | Layout.minimumWidth  | implicitWidth         | Layout.maximumWidth      |
|FALLBACK        | 0                    | width                 | Number.POSITIVE_INFINITY |
+----------------+----------------------+-----------------------+--------------------------+
 */
void QQuickLayout::effectiveSizeHints_helper(QQuickItem *item, QSizeF *cachedSizeHints, QQuickLayoutAttached **attachedInfo, bool useFallbackToWidthOrHeight)
{
    for (int i = 0; i < Qt::NSizeHints; ++i)
        cachedSizeHints[i] = QSizeF();
    QQuickLayoutAttached *info = attachedLayoutObject(item, false);
    // First, retrieve the user-specified hints from the attached "Layout." properties
    if (info) {
        struct Getters {
            SizeGetter call[NSizes];
        };

        static Getters horGetters = {
            {&QQuickLayoutAttached::minimumWidth, &QQuickLayoutAttached::preferredWidth, &QQuickLayoutAttached::maximumWidth},
        };

        static Getters verGetters = {
            {&QQuickLayoutAttached::minimumHeight, &QQuickLayoutAttached::preferredHeight, &QQuickLayoutAttached::maximumHeight}
        };
        for (int i = 0; i < NSizes; ++i) {
            SizeGetter getter = horGetters.call[i];
            Q_ASSERT(getter);

            if (info->isExtentExplicitlySet(Qt::Horizontal, (Qt::SizeHint)i))
                cachedSizeHints[i].setWidth((info->*getter)());

            getter = verGetters.call[i];
            Q_ASSERT(getter);
            if (info->isExtentExplicitlySet(Qt::Vertical, (Qt::SizeHint)i))
                cachedSizeHints[i].setHeight((info->*getter)());
        }
    }

    QSizeF &minS = cachedSizeHints[Qt::MinimumSize];
    QSizeF &prefS = cachedSizeHints[Qt::PreferredSize];
    QSizeF &maxS = cachedSizeHints[Qt::MaximumSize];
    QSizeF &descentS = cachedSizeHints[Qt::MinimumDescent];

    // For instance, will normalize the following user-set hints
    // from: [10, 5, 60]
    // to:   [10, 10, 60]
    normalizeHints(minS.rwidth(), prefS.rwidth(), maxS.rwidth(), descentS.rwidth());
    normalizeHints(minS.rheight(), prefS.rheight(), maxS.rheight(), descentS.rheight());

    // All explicit values gathered, now continue to gather the implicit sizes

    //--- GATHER MAXIMUM SIZE HINTS ---
    combineImplicitHints(info, Qt::MaximumSize, &maxS);
    combineSize(maxS, QSizeF(std::numeric_limits<qreal>::infinity(), std::numeric_limits<qreal>::infinity()));
    // implicit max or min sizes should not limit an explicitly set preferred size
    expandSize(maxS, prefS);
    expandSize(maxS, minS);

    //--- GATHER MINIMUM SIZE HINTS ---
    combineImplicitHints(info, Qt::MinimumSize, &minS);
    expandSize(minS, QSizeF(0,0));
    boundSize(minS, prefS);
    boundSize(minS, maxS);

    //--- GATHER PREFERRED SIZE HINTS ---
    // First, from implicitWidth/Height
    qreal &prefWidth = prefS.rwidth();
    qreal &prefHeight = prefS.rheight();
    if (prefWidth < 0 && item->implicitWidth() > 0)
        prefWidth = qCeil(item->implicitWidth());
    if (prefHeight < 0 &&  item->implicitHeight() > 0)
        prefHeight = qCeil(item->implicitHeight());

    // If that fails, make an ultimate fallback to width/height

    if (!info && (prefWidth < 0 || prefHeight < 0))
        info = attachedLayoutObject(item);

    if (useFallbackToWidthOrHeight && info) {
        /* This block is a bit hacky, but if we want to support using width/height
           as preferred size hints in layouts, (which we think most people expect),
           we only want to use the initial width.
           This is because the width will change due to layout rearrangement, and the preferred
           width should return the same value, regardless of the current width.
           We therefore store the width in the implicitWidth attached property.
           Since the layout listens to changes of implicitWidth, (it will
           basically cause an invalidation of the layout), we have to disable that
           notification while we set the implicit width (and height).

           Only use this fallback the first time the size hint is queried. Otherwise, we might
           end up picking a width that is different than what was specified in the QML.
        */
        if (prefWidth < 0 || prefHeight < 0) {
            item->blockSignals(true);
            if (prefWidth < 0) {
                prefWidth = item->width();
                item->setImplicitWidth(prefWidth);
            }
            if (prefHeight < 0) {
                prefHeight = item->height();
                item->setImplicitHeight(prefHeight);
            }
            item->blockSignals(false);
        }
    }



    // Normalize again after the implicit hints have been gathered
    expandSize(prefS, minS);
    boundSize(prefS, maxS);

    //--- GATHER DESCENT
    // Minimum descent is only applicable for the effective minimum height,
    // so we gather the descent last.
    const qreal minimumDescent = minS.height() - item->baselineOffset();
    descentS.setHeight(minimumDescent);

    if (info) {
        QMarginsF margins = info->qMargins();
        QSizeF extraMargins(margins.left() + margins.right(), margins.top() + margins.bottom());
        minS += extraMargins;
        prefS += extraMargins;
        maxS += extraMargins;
        descentS += extraMargins;
    }
    if (attachedInfo)
        *attachedInfo = info;
}

/*!
    \internal

    Assumes \a info is set (if the object has an attached property)
 */
QLayoutPolicy::Policy QQuickLayout::effectiveSizePolicy_helper(QQuickItem *item, Qt::Orientation orientation, QQuickLayoutAttached *info)
{
    bool fillExtent = false;
    bool isSet = false;
    if (info) {
        if (orientation == Qt::Horizontal) {
            isSet = info->isFillWidthSet();
            if (isSet) fillExtent = info->fillWidth();
        } else {
            isSet = info->isFillHeightSet();
            if (isSet) fillExtent = info->fillHeight();
        }
    }
    if (!isSet && qobject_cast<QQuickLayout*>(item))
        fillExtent = true;
    return fillExtent ? QLayoutPolicy::Preferred : QLayoutPolicy::Fixed;

}



QT_END_NAMESPACE
