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

#include "qquickstacklayout_p.h"
#include <limits>

/*!
    \qmltype StackLayout
    \instantiates QQuickStackLayout
    \inherits Item
    \inqmlmodule QtQuick.Layouts
    \ingroup layouts
    \brief The StackLayout class provides a stack of items where
    only one item is visible at a time.

    The current visible item can be modified by setting the \l currentIndex property.
    The index corresponds to the order of the StackLayout's children.

    In contrast to most other layouts, child Items' \l{Layout::fillWidth}{Layout.fillWidth} and \l{Layout::fillHeight}{Layout.fillHeight} properties
    default to \c true. As a consequence, child items are by default filled to match the size of the StackLayout as long as their
    \l{Layout::maximumWidth}{Layout.maximumWidth} or \l{Layout::maximumHeight}{Layout.maximumHeight} does not prevent it.

    Items are added to the layout by reparenting the item to the layout. Similarly, removal is done by reparenting the item from the layout.
    Both of these operations will affect the layout's \l count property.

    The following code will create a StackLayout where only the 'plum' rectangle is visible.
    \code
    StackLayout {
        id: layout
        anchors.fill: parent
        currentIndex: 1
        Rectangle {
            color: 'teal'
            implicitWidth: 200
            implicitHeight: 200
        }
        Rectangle {
            color: 'plum'
            implicitWidth: 300
            implicitHeight: 200
        }
    }
    \endcode

    Items in a StackLayout support these attached properties:
    \list
        \li \l{Layout::minimumWidth}{Layout.minimumWidth}
        \li \l{Layout::minimumHeight}{Layout.minimumHeight}
        \li \l{Layout::preferredWidth}{Layout.preferredWidth}
        \li \l{Layout::preferredHeight}{Layout.preferredHeight}
        \li \l{Layout::maximumWidth}{Layout.maximumWidth}
        \li \l{Layout::maximumHeight}{Layout.maximumHeight}
        \li \l{Layout::fillWidth}{Layout.fillWidth}
        \li \l{Layout::fillHeight}{Layout.fillHeight}
    \endlist

    Read more about attached properties \l{QML Object Attributes}{here}.
    \sa ColumnLayout
    \sa GridLayout
    \sa RowLayout
    \sa StackView
*/

QQuickStackLayout::QQuickStackLayout(QQuickItem *parent) :
    QQuickLayout(*new QQuickStackLayoutPrivate, parent)
{
}

/*!
    \qmlproperty int StackLayout::count

    This property holds the number of items that belong to the layout.

    Only items that are children of the StackLayout will be candidates for layouting.
*/
int QQuickStackLayout::count() const
{
    Q_D(const QQuickStackLayout);
    return d->count;
}

/*!
    \qmlproperty int StackLayout::currentIndex

    This property holds the index of the child item that is currently visible in the StackLayout.
    By default it will be \c -1 for an empty layout, otherwise the default is \c 0 (referring to the first item).
*/
int QQuickStackLayout::currentIndex() const
{
    Q_D(const QQuickStackLayout);
    return d->currentIndex;
}

void QQuickStackLayout::setCurrentIndex(int index)
{
    Q_D(QQuickStackLayout);
    if (index != d->currentIndex) {
        QQuickItem *prev = itemAt(d->currentIndex);
        QQuickItem *next = itemAt(index);
        d->currentIndex = index;
        d->explicitCurrentIndex = true;
        if (prev)
            prev->setVisible(false);
        if (next)
            next->setVisible(true);

        if (isComponentComplete()) {
            rearrange(QSizeF(width(), height()));
            emit currentIndexChanged();
        }
    }
}

void QQuickStackLayout::componentComplete()
{
    QQuickLayout::componentComplete();    // will call our geometryChange(), (where isComponentComplete() == true)

    updateLayoutItems();

    QQuickItem *par = parentItem();
    if (qobject_cast<QQuickLayout*>(par))
        return;

    rearrange(QSizeF(width(), height()));
}

QSizeF QQuickStackLayout::sizeHint(Qt::SizeHint whichSizeHint) const
{
    QSizeF &askingFor = m_cachedSizeHints[whichSizeHint];
    if (!askingFor.isValid()) {
        QSizeF &minS = m_cachedSizeHints[Qt::MinimumSize];
        QSizeF &prefS = m_cachedSizeHints[Qt::PreferredSize];
        QSizeF &maxS = m_cachedSizeHints[Qt::MaximumSize];

        minS = QSizeF(0,0);
        prefS = QSizeF(0,0);
        maxS = QSizeF(std::numeric_limits<qreal>::infinity(), std::numeric_limits<qreal>::infinity());

        const int count = itemCount();
        m_cachedItemSizeHints.resize(count);
        for (int i = 0; i < count; ++i) {
            SizeHints &hints = m_cachedItemSizeHints[i];
            QQuickStackLayout::collectItemSizeHints(itemAt(i), hints.array);
            minS = minS.expandedTo(hints.min());
            prefS = prefS.expandedTo(hints.pref());
            //maxS = maxS.boundedTo(hints.max());       // Can be resized to be larger than any of its items.
                                                        // This is the same as QStackLayout does it.
            // Not sure how descent makes sense here...
        }
    }
    return askingFor;
}

int QQuickStackLayout::indexOf(QQuickItem *childItem) const
{
    if (childItem) {
        int indexOfItem = 0;
        const auto items = childItems();
        for (QQuickItem *item : items) {
            if (shouldIgnoreItem(item))
                continue;
            if (childItem == item)
                return indexOfItem;
            ++indexOfItem;
        }
    }
    return -1;
}

QQuickItem *QQuickStackLayout::itemAt(int index) const
{
    const auto items = childItems();
    for (QQuickItem *item : items) {
        if (shouldIgnoreItem(item))
            continue;
        if (index == 0)
            return item;
        --index;
    }
    return 0;
}

int QQuickStackLayout::itemCount() const
{
    int count = 0;
    const auto items = childItems();
    for (QQuickItem *item : items) {
        if (shouldIgnoreItem(item))
            continue;
        ++count;
    }
    return count;
}

void QQuickStackLayout::setAlignment(QQuickItem * /*item*/, Qt::Alignment /*align*/)
{
    // ### Do we have to respect alignment?
}

void QQuickStackLayout::invalidate(QQuickItem *childItem)
{
    Q_D(QQuickStackLayout);
    if (d->m_ignoredItems.contains(childItem)) {
        // If an invalid item gets a valid size, it should be included, as it was added to the layout
        updateLayoutItems();
        return;
    }

    const int indexOfChild = indexOf(childItem);
    if (indexOfChild >= 0 && indexOfChild < m_cachedItemSizeHints.count()) {
        m_cachedItemSizeHints[indexOfChild].min() = QSizeF();
        m_cachedItemSizeHints[indexOfChild].pref() = QSizeF();
        m_cachedItemSizeHints[indexOfChild].max() = QSizeF();
    }

    for (int i = 0; i < Qt::NSizeHints; ++i)
        m_cachedSizeHints[i] = QSizeF();
    QQuickLayout::invalidate(this);

    QQuickLayoutAttached *info = attachedLayoutObject(this);

    const QSizeF min = sizeHint(Qt::MinimumSize);
    const QSizeF pref = sizeHint(Qt::PreferredSize);
    const QSizeF max = sizeHint(Qt::MaximumSize);

    const bool old = info->setChangesNotificationEnabled(false);
    info->setMinimumImplicitSize(min);
    info->setMaximumImplicitSize(max);
    info->setChangesNotificationEnabled(old);
    if (pref.width() == implicitWidth() && pref.height() == implicitHeight()) {
        // In case setImplicitSize does not emit implicit{Width|Height}Changed
        if (QQuickLayout *parentLayout = qobject_cast<QQuickLayout *>(parentItem()))
            parentLayout->invalidate(this);
    } else {
        setImplicitSize(pref.width(), pref.height());
    }
}

void QQuickStackLayout::updateLayoutItems()
{
    Q_D(QQuickStackLayout);
    d->m_ignoredItems.clear();
    const int count = itemCount();
    int oldIndex = d->currentIndex;
    if (!d->explicitCurrentIndex)
        d->currentIndex = (count > 0 ? 0 : -1);

    if (d->currentIndex != oldIndex)
        emit currentIndexChanged();

    if (count != d->count) {
        d->count = count;
        emit countChanged();
    }
    for (int i = 0; i < count; ++i)
        itemAt(i)->setVisible(d->currentIndex == i);

    invalidate();
}

void QQuickStackLayout::rearrange(const QSizeF &newSize)
{
    Q_D(QQuickStackLayout);
    if (newSize.isNull() || !newSize.isValid())
        return;
    (void)sizeHint(Qt::PreferredSize);  // Make sure m_cachedItemSizeHints are valid

    if (d->currentIndex == -1 || d->currentIndex >= m_cachedItemSizeHints.count())
        return;
    QQuickStackLayout::SizeHints &hints = m_cachedItemSizeHints[d->currentIndex];
    QQuickItem *item = itemAt(d->currentIndex);
    Q_ASSERT(item);
    item->setPosition(QPointF(0,0));    // ### respect alignment?
    item->setSize(newSize.expandedTo(hints.min()).boundedTo(hints.max()));
    QQuickLayout::rearrange(newSize);
}

void QQuickStackLayout::collectItemSizeHints(QQuickItem *item, QSizeF *sizeHints)
{
    QQuickLayoutAttached *info = 0;
    QQuickLayout::effectiveSizeHints_helper(item, sizeHints, &info, true);
    if (!info)
        return;
    if (info->isFillWidthSet() && !info->fillWidth()) {
        const qreal pref = sizeHints[Qt::PreferredSize].width();
        sizeHints[Qt::MinimumSize].setWidth(pref);
        sizeHints[Qt::MaximumSize].setWidth(pref);
    }

    if (info->isFillHeightSet() && !info->fillHeight()) {
        const qreal pref = sizeHints[Qt::PreferredSize].height();
        sizeHints[Qt::MinimumSize].setHeight(pref);
        sizeHints[Qt::MaximumSize].setHeight(pref);
    }
}

bool QQuickStackLayout::shouldIgnoreItem(QQuickItem *item) const
{
    const bool ignored = QQuickItemPrivate::get(item)->isTransparentForPositioner();
    if (ignored)
        d_func()->m_ignoredItems << item;
    return ignored;
}
