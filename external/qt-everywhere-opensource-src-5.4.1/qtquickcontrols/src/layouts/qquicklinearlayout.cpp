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

#include "qquicklinearlayout_p.h"
#include "qquickgridlayoutengine_p.h"
#include "qquicklayoutstyleinfo_p.h"
#include <QtCore/qnumeric.h>
#include "qdebug.h"
#include <limits>

/*!
    \qmltype RowLayout
    \instantiates QQuickRowLayout
    \inherits Item
    \inqmlmodule QtQuick.Layouts
    \ingroup layouts
    \brief Identical to \l GridLayout, but having only one row.

    It is available as a convenience for developers, as it offers a cleaner API.

    Items in a RowLayout support these attached properties:
    \list
        \li \l{Layout::minimumWidth}{Layout.minimumWidth}
        \li \l{Layout::minimumHeight}{Layout.minimumHeight}
        \li \l{Layout::preferredWidth}{Layout.preferredWidth}
        \li \l{Layout::preferredHeight}{Layout.preferredHeight}
        \li \l{Layout::maximumWidth}{Layout.maximumWidth}
        \li \l{Layout::maximumHeight}{Layout.maximumHeight}
        \li \l{Layout::fillWidth}{Layout.fillWidth}
        \li \l{Layout::fillHeight}{Layout.fillHeight}
        \li \l{Layout::alignment}{Layout.alignment}
    \endlist

    \image rowlayout.png

    \code
    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 6
        Rectangle {
            color: 'teal'
            Layout.fillWidth: true
            Layout.minimumWidth: 50
            Layout.preferredWidth: 100
            Layout.maximumWidth: 300
            Layout.minimumHeight: 150
            Text {
                anchors.centerIn: parent
                text: parent.width + 'x' + parent.height
            }
        }
        Rectangle {
            color: 'plum'
            Layout.fillWidth: true
            Layout.minimumWidth: 100
            Layout.preferredWidth: 200
            Layout.preferredHeight: 100
            Text {
                anchors.centerIn: parent
                text: parent.width + 'x' + parent.height
            }
        }
    }
    \endcode

    Read more about attached properties \l{QML Object Attributes}{here}.
    \sa ColumnLayout
    \sa GridLayout
    \sa Row
*/

/*!
    \qmltype ColumnLayout
    \instantiates QQuickColumnLayout
    \inherits Item
    \inqmlmodule QtQuick.Layouts
    \ingroup layouts
    \brief Identical to \l GridLayout, but having only one column.

    It is available as a convenience for developers, as it offers a cleaner API.

    Items in a ColumnLayout support these attached properties:
    \list
        \li \l{Layout::minimumWidth}{Layout.minimumWidth}
        \li \l{Layout::minimumHeight}{Layout.minimumHeight}
        \li \l{Layout::preferredWidth}{Layout.preferredWidth}
        \li \l{Layout::preferredHeight}{Layout.preferredHeight}
        \li \l{Layout::maximumWidth}{Layout.maximumWidth}
        \li \l{Layout::maximumHeight}{Layout.maximumHeight}
        \li \l{Layout::fillWidth}{Layout.fillWidth}
        \li \l{Layout::fillHeight}{Layout.fillHeight}
        \li \l{Layout::alignment}{Layout.alignment}
    \endlist

    \image columnlayout.png

    \code
    ColumnLayout{
        spacing: 2

        Rectangle {
            Layout.alignment: Qt.AlignCenter
            color: "red"
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
        }

        Rectangle {
            Layout.alignment: Qt.AlignRight
            color: "green"
            Layout.preferredWidth: 40
            Layout.preferredHeight: 70
        }

        Rectangle {
            Layout.alignment: Qt.AlignBottom
            Layout.fillHeight: true
            color: "blue"
            Layout.preferredWidth: 70
            Layout.preferredHeight: 40
        }
    }
    \endcode

    Read more about attached properties \l{QML Object Attributes}{here}.

    \sa RowLayout
    \sa GridLayout
    \sa Column
*/


/*!
    \qmltype GridLayout
    \instantiates QQuickGridLayout
    \inherits Item
    \inqmlmodule QtQuick.Layouts
    \ingroup layouts
    \brief Provides a way of dynamically arranging items in a grid.



    If the GridLayout is resized, all items in the layout will be rearranged. It is similar
    to the widget-based QGridLayout. All visible children of the GridLayout element will belong to
    the layout. If you want a layout with just one row or one column, you can use the
    \l RowLayout or \l ColumnLayout. These offer a bit more convenient API, and improve
    readability.

    By default items will be arranged according to the \l flow property. The default value of
    the \l flow property is \c GridLayout.LeftToRight.

    If the \l columns property is specified, it will be treated as a maximum limit of how many
    columns the layout can have, before the auto-positioning wraps back to the beginning of the
    next row. The \l columns property is only used when \l flow is  \c GridLayout.LeftToRight.

    \image gridlayout.png

    \code
    GridLayout {
        id: grid
        columns: 3

        Text { text: "Three"; font.bold: true; }
        Text { text: "words"; color: "red" }
        Text { text: "in"; font.underline: true }
        Text { text: "a"; font.pixelSize: 20 }
        Text { text: "row"; font.strikeout: true }
    }
    \endcode

    The \l rows property works in a similar way, but items are auto-positioned vertically. The \l
    rows property is only used when \l flow is \c GridLayout.TopToBottom.

    You can specify which cell you want an item to occupy by setting the
    \l{Layout::row}{Layout.row} and \l{Layout::column}{Layout.column} properties. You can also
    specify the row span or column span by setting the \l{Layout::rowSpan}{Layout.rowSpan} or
    \l{Layout::columnSpan}{Layout.columnSpan} properties.


    Items in a GridLayout support these attached properties:
    \list
        \li \l{Layout::row}{Layout.row}
        \li \l{Layout::column}{Layout.column}
        \li \l{Layout::rowSpan}{Layout.rowSpan}
        \li \l{Layout::columnSpan}{Layout.columnSpan}
        \li \l{Layout::minimumWidth}{Layout.minimumWidth}
        \li \l{Layout::minimumHeight}{Layout.minimumHeight}
        \li \l{Layout::preferredWidth}{Layout.preferredWidth}
        \li \l{Layout::preferredHeight}{Layout.preferredHeight}
        \li \l{Layout::maximumWidth}{Layout.maximumWidth}
        \li \l{Layout::maximumHeight}{Layout.maximumHeight}
        \li \l{Layout::fillWidth}{Layout.fillWidth}
        \li \l{Layout::fillHeight}{Layout.fillHeight}
        \li \l{Layout::alignment}{Layout.alignment}
    \endlist

    Read more about attached properties \l{QML Object Attributes}{here}.

    \sa RowLayout
    \sa ColumnLayout
    \sa Grid
*/



QT_BEGIN_NAMESPACE

QQuickGridLayoutBase::QQuickGridLayoutBase()
    : QQuickLayout(*new QQuickGridLayoutBasePrivate)
{

}

QQuickGridLayoutBase::QQuickGridLayoutBase(QQuickGridLayoutBasePrivate &dd,
                                           Qt::Orientation orientation,
                                           QQuickItem *parent /*= 0*/)
    : QQuickLayout(dd, parent)
{
    Q_D(QQuickGridLayoutBase);
    d->orientation = orientation;
    d->styleInfo = new QQuickLayoutStyleInfo;
}

Qt::Orientation QQuickGridLayoutBase::orientation() const
{
    Q_D(const QQuickGridLayoutBase);
    return d->orientation;
}

void QQuickGridLayoutBase::setOrientation(Qt::Orientation orientation)
{
    Q_D(QQuickGridLayoutBase);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    invalidate();
}

QSizeF QQuickGridLayoutBase::sizeHint(Qt::SizeHint whichSizeHint) const
{
    Q_D(const QQuickGridLayoutBase);
    return d->engine.sizeHint(whichSizeHint, QSizeF(), d->styleInfo);
}

/*!
    \qmlproperty enumeration GridLayout::layoutDirection
    \since QtQuick.Layouts 1.1

    This property holds the layout direction of the grid layout - it controls whether items are
    laid out from left to right or right to left. If \c Qt.RightToLeft is specified,
    left-aligned items will be right-aligned and right-aligned items will be left-aligned.

    Possible values:

    \list
    \li Qt.LeftToRight (default) - Items are laid out from left to right.
    \li Qt.RightToLeft - Items are laid out from right to left.
    \endlist

    \sa RowLayout::layoutDirection, ColumnLayout::layoutDirection
*/
Qt::LayoutDirection QQuickGridLayoutBase::layoutDirection() const
{
    Q_D(const QQuickGridLayoutBase);
    return d->m_layoutDirection;
}

void QQuickGridLayoutBase::setLayoutDirection(Qt::LayoutDirection dir)
{
    Q_D(QQuickGridLayoutBase);
    d->m_layoutDirection = dir;
    invalidate();
}

Qt::LayoutDirection QQuickGridLayoutBase::effectiveLayoutDirection() const
{
    Q_D(const QQuickGridLayoutBase);
    return !d->effectiveLayoutMirror == (layoutDirection() == Qt::LeftToRight)
                                      ? Qt::LeftToRight : Qt::RightToLeft;
}

void QQuickGridLayoutBase::setAlignment(QQuickItem *item, Qt::Alignment alignment)
{
    Q_D(QQuickGridLayoutBase);
    d->engine.setAlignment(item, alignment);
}

QQuickGridLayoutBase::~QQuickGridLayoutBase()
{
    Q_D(QQuickGridLayoutBase);
    d->m_isReady = false;

    /* Avoid messy deconstruction, should give:
        * Faster deconstruction
        * Less risk of signals reaching already deleted objects
    */
    for (int i = 0; i < itemCount(); ++i) {
        QQuickItem *item = itemAt(i);
        QObject::disconnect(item, SIGNAL(destroyed()), this, SLOT(onItemDestroyed()));
        QObject::disconnect(item, SIGNAL(visibleChanged()), this, SLOT(onItemVisibleChanged()));
        QObject::disconnect(item, SIGNAL(implicitWidthChanged()), this, SLOT(invalidateSenderItem()));
        QObject::disconnect(item, SIGNAL(implicitHeightChanged()), this, SLOT(invalidateSenderItem()));
    }
    delete d->styleInfo;
}

void QQuickGridLayoutBase::componentComplete()
{
    Q_D(QQuickGridLayoutBase);
    quickLayoutDebug() << objectName() << "QQuickGridLayoutBase::componentComplete()" << parent();
    d->m_disableRearrange = true;
    QQuickLayout::componentComplete();    // will call our geometryChange(), (where isComponentComplete() == true)
    d->m_isReady = true;
    d->m_disableRearrange = false;
    updateLayoutItems();

    QQuickItem *par = parentItem();
    if (qobject_cast<QQuickLayout*>(par))
        return;
    rearrange(QSizeF(width(), height()));
}

/*
  Invalidation happens like this as a reaction to that a size hint changes on an item "a":

  Suppose we have the following Qml document:
    RowLayout {
        id: l1
        RowLayout {
            id: l2
            Item {
                id: a
            }
            Item {
                id: b
            }
        }
    }

  1.    l2->invalidateChildItem(a) is called on l2, where item refers to "a".
        (this will dirty the cached size hints of item "a")
  2.    l2->invalidate() is called
        this will :
            i)  invalidate the layout engine
            ii) dirty the cached size hints of item "l2" (by calling parentLayout()->invalidateChildItem

 */
/*!
   \internal

    Invalidates \a childItem and this layout.
    After a call to invalidate, the next call to retrieve e.g. sizeHint will be up-to date.
    This function will also call QQuickLayout::invalidate(0), to ensure that the parent layout
    is invalidated.
 */
void QQuickGridLayoutBase::invalidate(QQuickItem *childItem)
{
    Q_D(QQuickGridLayoutBase);
    if (!isReady())
        return;
    quickLayoutDebug() << "QQuickGridLayoutBase::invalidate()";

    if (childItem) {
        if (QQuickGridLayoutItem *layoutItem = d->engine.findLayoutItem(childItem))
            layoutItem->invalidate();
        if (d->m_ignoredItems.contains(childItem)) {
            updateLayoutItems();
            return;
        }
    }
    // invalidate engine
    d->engine.invalidate();

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

void QQuickGridLayoutBase::updateLayoutItems()
{
    Q_D(QQuickGridLayoutBase);
    if (!isReady())
        return;
    quickLayoutDebug() << "QQuickGridLayoutBase::updateLayoutItems";
    d->engine.deleteItems();
    insertLayoutItems();

    invalidate();
    quickLayoutDebug() << "QQuickGridLayoutBase::updateLayoutItems LEAVING";
}

QQuickItem *QQuickGridLayoutBase::itemAt(int index) const
{
    Q_D(const QQuickGridLayoutBase);
    return static_cast<QQuickGridLayoutItem*>(d->engine.itemAt(index))->layoutItem();
}

int QQuickGridLayoutBase::itemCount() const
{
    Q_D(const QQuickGridLayoutBase);
    return d->engine.itemCount();
}

void QQuickGridLayoutBase::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == ItemChildAddedChange) {
        quickLayoutDebug() << "ItemChildAddedChange";
        QQuickItem *item = value.item;
        QObject::connect(item, SIGNAL(destroyed()), this, SLOT(onItemDestroyed()));
        QObject::connect(item, SIGNAL(visibleChanged()), this, SLOT(onItemVisibleChanged()));
        QObject::connect(item, SIGNAL(implicitWidthChanged()), this, SLOT(invalidateSenderItem()));
        QObject::connect(item, SIGNAL(implicitHeightChanged()), this, SLOT(invalidateSenderItem()));
        QObject::connect(item, SIGNAL(baselineOffsetChanged(qreal)), this, SLOT(invalidateSenderItem()));

        if (isReady())
            updateLayoutItems();
    } else if (change == ItemChildRemovedChange) {
        quickLayoutDebug() << "ItemChildRemovedChange";
        QQuickItem *item = value.item;
        QObject::disconnect(item, SIGNAL(destroyed()), this, SLOT(onItemDestroyed()));
        QObject::disconnect(item, SIGNAL(visibleChanged()), this, SLOT(onItemVisibleChanged()));
        QObject::disconnect(item, SIGNAL(implicitWidthChanged()), this, SLOT(invalidateSenderItem()));
        QObject::disconnect(item, SIGNAL(implicitHeightChanged()), this, SLOT(invalidateSenderItem()));
        QObject::disconnect(item, SIGNAL(baselineOffsetChanged(qreal)), this, SLOT(invalidateSenderItem()));
        if (isReady())
            updateLayoutItems();
    }

    QQuickLayout::itemChange(change, value);
}

void QQuickGridLayoutBase::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickGridLayoutBase);
    QQuickLayout::geometryChanged(newGeometry, oldGeometry);
    if (d->m_disableRearrange || !isReady() || !newGeometry.isValid())
        return;
    quickLayoutDebug() << "QQuickGridLayoutBase::geometryChanged" << newGeometry << oldGeometry;
    rearrange(newGeometry.size());
}

void QQuickGridLayoutBase::removeGridItem(QGridLayoutItem *gridItem)
{
    Q_D(QQuickGridLayoutBase);
    const int index = gridItem->firstRow(d->orientation);
    d->engine.removeItem(gridItem);
    d->engine.removeRows(index, 1, d->orientation);
}

bool QQuickGridLayoutBase::isReady() const
{
    return d_func()->m_isReady;
}

void QQuickGridLayoutBase::removeLayoutItem(QQuickItem *item)
{
    Q_D(QQuickGridLayoutBase);
    quickLayoutDebug() << "QQuickGridLayoutBase::removeLayoutItem";
    if (QQuickGridLayoutItem *gridItem = d->engine.findLayoutItem(item)) {
        removeGridItem(gridItem);
        delete gridItem;
        invalidate();
    }
}

void QQuickGridLayoutBase::onItemVisibleChanged()
{
    if (!isReady())
        return;
    quickLayoutDebug() << "QQuickGridLayoutBase::onItemVisibleChanged";
    updateLayoutItems();
}

void QQuickGridLayoutBase::onItemDestroyed()
{
    if (!isReady())
        return;
    Q_D(QQuickGridLayoutBase);
    quickLayoutDebug() << "QQuickGridLayoutBase::onItemDestroyed";
    QQuickItem *inDestruction = static_cast<QQuickItem *>(sender());
    if (QQuickGridLayoutItem *gridItem = d->engine.findLayoutItem(inDestruction)) {
        removeGridItem(gridItem);
        delete gridItem;
        invalidate();
    }
}

void QQuickGridLayoutBase::invalidateSenderItem()
{
    if (!isReady())
        return;
    QQuickItem *item = static_cast<QQuickItem *>(sender());
    Q_ASSERT(item);
    invalidate(item);
}

void QQuickGridLayoutBase::rearrange(const QSizeF &size)
{
    Q_D(QQuickGridLayoutBase);
    if (!isReady())
        return;

    quickLayoutDebug() << objectName() << "QQuickGridLayoutBase::rearrange()" << size;
    Qt::LayoutDirection visualDir = effectiveLayoutDirection();
    d->engine.setVisualDirection(visualDir);

    /*
    qreal left, top, right, bottom;
    left = top = right = bottom = 0;                    // ### support for margins?
    if (visualDir == Qt::RightToLeft)
        qSwap(left, right);
    */

    // Set m_dirty to false in case size hint changes during arrangement.
    // This could happen if there is a binding like implicitWidth: height
    QQuickLayout::rearrange(size);
    d->engine.setGeometries(QRectF(QPointF(0,0), size), d->styleInfo);
}

bool QQuickGridLayoutBase::shouldIgnoreItem(QQuickItem *child, QQuickLayoutAttached *&info, QSizeF *sizeHints)
{
    Q_D(QQuickGridLayoutBase);
    bool ignoreItem = true;
    QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);
    if (childPrivate->explicitVisible) {
        QQuickGridLayoutItem::effectiveSizeHints_helper(child, sizeHints, &info, true);
        QSizeF effectiveMaxSize = sizeHints[Qt::MaximumSize];
        if (!effectiveMaxSize.isNull()) {
            QSizeF &prefS = sizeHints[Qt::PreferredSize];
            if (QQuickGridLayoutItem::effectiveSizePolicy_helper(child, Qt::Horizontal, info) == QLayoutPolicy::Fixed)
                effectiveMaxSize.setWidth(prefS.width());
            if (QQuickGridLayoutItem::effectiveSizePolicy_helper(child, Qt::Vertical, info) == QLayoutPolicy::Fixed)
                effectiveMaxSize.setHeight(prefS.height());
        }
        ignoreItem = effectiveMaxSize.isNull();
    }

    if (ignoreItem)
        d->m_ignoredItems << child;
    return ignoreItem;
}

/**********************************
 **
 ** QQuickGridLayout
 **
 **/
QQuickGridLayout::QQuickGridLayout(QQuickItem *parent /* = 0*/)
    : QQuickGridLayoutBase(*new QQuickGridLayoutPrivate, Qt::Horizontal, parent)
{
}

/*!
    \qmlproperty real GridLayout::columnSpacing

    This property holds the spacing between each column.
    The default value is \c 5.
*/
qreal QQuickGridLayout::columnSpacing() const
{
    Q_D(const QQuickGridLayout);
    return d->engine.spacing(Qt::Horizontal, d->styleInfo);
}

void QQuickGridLayout::setColumnSpacing(qreal spacing)
{
    Q_D(QQuickGridLayout);
    if (qIsNaN(spacing) || columnSpacing() == spacing)
        return;

    d->engine.setSpacing(spacing, Qt::Horizontal);
    invalidate();
}

/*!
    \qmlproperty real GridLayout::rowSpacing

    This property holds the spacing between each row.
    The default value is \c 5.
*/
qreal QQuickGridLayout::rowSpacing() const
{
    Q_D(const QQuickGridLayout);
    return d->engine.spacing(Qt::Vertical, d->styleInfo);
}

void QQuickGridLayout::setRowSpacing(qreal spacing)
{
    Q_D(QQuickGridLayout);
    if (qIsNaN(spacing) || rowSpacing() == spacing)
        return;

    d->engine.setSpacing(spacing, Qt::Vertical);
    invalidate();
}

/*!
    \qmlproperty int GridLayout::columns

    This property holds the column limit for items positioned if \l flow is
    \c GridLayout.LeftToRight.
    The default value is that there is no limit.
*/
int QQuickGridLayout::columns() const
{
    Q_D(const QQuickGridLayout);
    return d->columns;
}

void QQuickGridLayout::setColumns(int columns)
{
    Q_D(QQuickGridLayout);
    if (d->columns == columns)
        return;
    d->columns = columns;
    updateLayoutItems();
    emit columnsChanged();
}


/*!
    \qmlproperty int GridLayout::rows

    This property holds the row limit for items positioned if \l flow is \c GridLayout.TopToBottom.
    The default value is that there is no limit.
*/
int QQuickGridLayout::rows() const
{
    Q_D(const QQuickGridLayout);
    return d->rows;
}

void QQuickGridLayout::setRows(int rows)
{
    Q_D(QQuickGridLayout);
    if (d->rows == rows)
        return;
    d->rows = rows;
    updateLayoutItems();
    emit rowsChanged();
}


/*!
    \qmlproperty enumeration GridLayout::flow

    This property holds the flow direction of items that does not have an explicit cell
    position set.
    It is used together with the \l columns or \l rows property, where
    they specify when flow is reset to the next row or column respectively.

    Possible values are:

    \list
    \li GridLayout.LeftToRight (default) - Items are positioned next to
       each other, then wrapped to the next line.
    \li GridLayout.TopToBottom - Items are positioned next to each
       other from top to bottom, then wrapped to the next column.
    \endlist

    \sa rows
    \sa columns
*/
QQuickGridLayout::Flow QQuickGridLayout::flow() const
{
    Q_D(const QQuickGridLayout);
    return d->flow;
}

void QQuickGridLayout::setFlow(QQuickGridLayout::Flow flow)
{
    Q_D(QQuickGridLayout);
    if (d->flow == flow)
        return;
    d->flow = flow;
    // If flow is changed, the layout needs to be repopulated
    updateLayoutItems();
    emit flowChanged();
}

void QQuickGridLayout::insertLayoutItems()
{
    Q_D(QQuickGridLayout);

    int nextCellPos[2] = {0,0};
    int &nextColumn = nextCellPos[0];
    int &nextRow = nextCellPos[1];

    const int flowOrientation = flow();
    int &flowColumn = nextCellPos[flowOrientation];
    int &flowRow = nextCellPos[1 - flowOrientation];
    int flowBound = (flowOrientation == QQuickGridLayout::LeftToRight) ? columns() : rows();

    if (flowBound < 0)
        flowBound = std::numeric_limits<int>::max();

    d->m_ignoredItems.clear();
    QSizeF sizeHints[Qt::NSizeHints];
    foreach (QQuickItem *child,  childItems()) {
        QQuickLayoutAttached *info = 0;

        // Will skip all items with effective maximum width/height == 0
        if (shouldIgnoreItem(child, info, sizeHints))
            continue;

        Qt::Alignment alignment = 0;
        int row = -1;
        int column = -1;
        int span[2] = {1,1};
        int &columnSpan = span[0];
        int &rowSpan = span[1];

        bool invalidRowColumn = false;
        if (info) {
            if (info->isRowSet() || info->isColumnSet()) {
                // If row is specified and column is not specified (or vice versa),
                // the unspecified component of the cell position should default to 0
                row = column = 0;
                if (info->isRowSet()) {
                    row = info->row();
                    invalidRowColumn = row < 0;
                }
                if (info->isColumnSet()) {
                    column = info->column();
                    invalidRowColumn = column < 0;
                }
            }
            if (invalidRowColumn) {
                qWarning("QQuickGridLayoutBase::insertLayoutItems: invalid row/column: %d",
                         row < 0 ? row : column);
                return;
            }
            rowSpan = info->rowSpan();
            columnSpan = info->columnSpan();
            if (columnSpan < 1 || rowSpan < 1) {
                qWarning("QQuickGridLayoutBase::addItem: invalid row span/column span: %d",
                         rowSpan < 1 ? rowSpan : columnSpan);
                return;
            }

            alignment = info->alignment();
        }

        Q_ASSERT(columnSpan >= 1);
        Q_ASSERT(rowSpan >= 1);
        const int sp = span[flowOrientation];
        if (sp > flowBound)
            return;

        if (row >= 0)
            nextRow = row;
        if (column >= 0)
            nextColumn = column;

        if (row < 0 || column < 0) {
            /* if row or column is not specified, find next position by
               advancing in the flow direction until there is a cell that
               can accept the item.

               The acceptance rules are pretty simple, but complexity arises
               when an item requires several cells (due to spans):
               1. Check if the cells that the item will require
                  does not extend beyond columns (for LeftToRight) or
                  rows (for TopToBottom).
               2. Check if the cells that the item will require is not already
                  taken by another item.
            */
            bool cellAcceptsItem;
            while (true) {
                // Check if the item does not span beyond the layout bound
                cellAcceptsItem = (flowColumn + sp) <= flowBound;

                // Check if all the required cells are not taken
                for (int rs = 0; cellAcceptsItem && rs < rowSpan; ++rs) {
                    for (int cs = 0; cellAcceptsItem && cs < columnSpan; ++cs) {
                        if (d->engine.itemAt(nextRow + rs, nextColumn + cs)) {
                            cellAcceptsItem = false;
                        }
                    }
                }
                if (cellAcceptsItem)
                    break;
                ++flowColumn;
                if (flowColumn == flowBound) {
                    flowColumn = 0;
                    ++flowRow;
                }
            }
        }
        column = nextColumn;
        row = nextRow;
        QQuickGridLayoutItem *layoutItem = new QQuickGridLayoutItem(child, row, column, rowSpan, columnSpan, alignment);
        layoutItem->setCachedSizeHints(sizeHints);

        d->engine.insertItem(layoutItem, -1);
    }
}

/**********************************
 **
 ** QQuickLinearLayout
 **
 **/
QQuickLinearLayout::QQuickLinearLayout(Qt::Orientation orientation,
                                        QQuickItem *parent /*= 0*/)
    : QQuickGridLayoutBase(*new QQuickLinearLayoutPrivate, orientation, parent)
{
}

/*!
    \qmlproperty enumeration RowLayout::layoutDirection
    \since QtQuick.Layouts 1.1

    This property holds the layout direction of the row layout - it controls whether items are laid
    out from left ro right or right to left. If \c Qt.RightToLeft is specified,
    left-aligned items will be right-aligned and right-aligned items will be left-aligned.

    Possible values:

    \list
    \li Qt.LeftToRight (default) - Items are laid out from left to right.
    \li Qt.RightToLeft - Items are laid out from right to left
    \endlist

    \sa GridLayout::layoutDirection, ColumnLayout::layoutDirection
*/
/*!
    \qmlproperty enumeration ColumnLayout::layoutDirection
    \since QtQuick.Layouts 1.1

    This property holds the layout direction of the column layout - it controls whether items are laid
    out from left ro right or right to left. If \c Qt.RightToLeft is specified,
    left-aligned items will be right-aligned and right-aligned items will be left-aligned.

    Possible values:

    \list
    \li Qt.LeftToRight (default) - Items are laid out from left to right.
    \li Qt.RightToLeft - Items are laid out from right to left
    \endlist

    \sa GridLayout::layoutDirection, RowLayout::layoutDirection
*/


/*!
    \qmlproperty real RowLayout::spacing

    This property holds the spacing between each cell.
    The default value is \c 5.
*/
/*!
    \qmlproperty real ColumnLayout::spacing

    This property holds the spacing between each cell.
    The default value is \c 5.
*/

qreal QQuickLinearLayout::spacing() const
{
    Q_D(const QQuickLinearLayout);
    return d->engine.spacing(d->orientation, d->styleInfo);
}

void QQuickLinearLayout::setSpacing(qreal space)
{
    Q_D(QQuickLinearLayout);
    if (qIsNaN(space) || spacing() == space)
        return;

    d->engine.setSpacing(space, Qt::Horizontal | Qt::Vertical);
    invalidate();
}

void QQuickLinearLayout::insertLayoutItems()
{
    Q_D(QQuickLinearLayout);
    d->m_ignoredItems.clear();
    QSizeF sizeHints[Qt::NSizeHints];
    foreach (QQuickItem *child,  childItems()) {
        Q_ASSERT(child);
        QQuickLayoutAttached *info = 0;

        // Will skip all items with effective maximum width/height == 0
        if (shouldIgnoreItem(child, info, sizeHints))
            continue;

        Qt::Alignment alignment = 0;
        if (info)
            alignment = info->alignment();

        const int index = d->engine.rowCount(d->orientation);
        d->engine.insertRow(index, d->orientation);

        int gridRow = 0;
        int gridColumn = index;
        if (d->orientation == Qt::Vertical)
            qSwap(gridRow, gridColumn);
        QQuickGridLayoutItem *layoutItem = new QQuickGridLayoutItem(child, gridRow, gridColumn, 1, 1, alignment);
        layoutItem->setCachedSizeHints(sizeHints);
        d->engine.insertItem(layoutItem, index);
    }
}

QT_END_NAMESPACE
