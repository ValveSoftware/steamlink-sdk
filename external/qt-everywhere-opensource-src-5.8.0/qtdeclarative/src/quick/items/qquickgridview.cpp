/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickgridview_p.h"
#include "qquickflickable_p_p.h"
#include "qquickitemview_p_p.h"

#include <private/qqmlobjectmodel_p.h>
#include <private/qquicksmoothedanimation_p_p.h>

#include <QtGui/qevent.h>
#include <QtCore/qmath.h>
#include <QtCore/qcoreapplication.h>
#include "qplatformdefs.h"

#include <cmath>

QT_BEGIN_NAMESPACE

#ifndef QML_FLICK_SNAPONETHRESHOLD
#define QML_FLICK_SNAPONETHRESHOLD 30
#endif

//----------------------------------------------------------------------------

class FxGridItemSG : public FxViewItem
{
public:
    FxGridItemSG(QQuickItem *i, QQuickGridView *v, bool own) : FxViewItem(i, v, own, static_cast<QQuickItemViewAttached*>(qmlAttachedPropertiesObject<QQuickGridView>(i))), view(v)
    {
    }

    qreal position() const Q_DECL_OVERRIDE {
        return rowPos();
    }

    qreal endPosition() const Q_DECL_OVERRIDE {
        return endRowPos();
    }

    qreal size() const Q_DECL_OVERRIDE {
        return view->flow() == QQuickGridView::FlowLeftToRight ? view->cellHeight() : view->cellWidth();
    }

    qreal sectionSize() const Q_DECL_OVERRIDE {
        return 0.0;
    }

    qreal rowPos() const {
        if (view->flow() == QQuickGridView::FlowLeftToRight)
            return (view->verticalLayoutDirection() == QQuickItemView::BottomToTop ? -view->cellHeight()-itemY() : itemY());
        else
            return (view->effectiveLayoutDirection() == Qt::RightToLeft ? -view->cellWidth()-itemX() : itemX());
    }

    qreal colPos() const {
        if (view->flow() == QQuickGridView::FlowLeftToRight) {
            if (view->effectiveLayoutDirection() == Qt::RightToLeft) {
                qreal colSize = view->cellWidth();
                int columns = view->width()/colSize;
                return colSize * (columns-1) - itemX();
            } else {
                return itemX();
            }
        } else {
            if (view->verticalLayoutDirection() == QQuickItemView::BottomToTop) {
                return -view->cellHeight() - itemY();
            } else {
                return itemY();
            }
        }
    }
    qreal endRowPos() const {
        if (view->flow() == QQuickGridView::FlowLeftToRight) {
            if (view->verticalLayoutDirection() == QQuickItemView::BottomToTop)
                return -itemY();
            else
                return itemY() + view->cellHeight();
        } else {
            if (view->effectiveLayoutDirection() == Qt::RightToLeft)
                return -itemX();
            else
                return itemX() + view->cellWidth();
        }
    }
    void setPosition(qreal col, qreal row, bool immediate = false) {
        moveTo(pointForPosition(col, row), immediate);
    }
    bool contains(qreal x, qreal y) const Q_DECL_OVERRIDE {
        return (x >= itemX() && x < itemX() + view->cellWidth() &&
                y >= itemY() && y < itemY() + view->cellHeight());
    }

    QQuickGridView *view;

private:
    QPointF pointForPosition(qreal col, qreal row) const {
        qreal x;
        qreal y;
        if (view->flow() == QQuickGridView::FlowLeftToRight) {
            x = col;
            y = row;
            if (view->effectiveLayoutDirection() == Qt::RightToLeft) {
                int columns = view->width()/view->cellWidth();
                x = view->cellWidth() * (columns-1) - col;
            }
        } else {
            x = row;
            y = col;
            if (view->effectiveLayoutDirection() == Qt::RightToLeft)
                x = -view->cellWidth() - row;
        }
        if (view->verticalLayoutDirection() == QQuickItemView::BottomToTop)
            y = -view->cellHeight() - y;
        return QPointF(x, y);
    }
};

//----------------------------------------------------------------------------

class QQuickGridViewPrivate : public QQuickItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickGridView)

public:
    Qt::Orientation layoutOrientation() const Q_DECL_OVERRIDE;
    bool isContentFlowReversed() const Q_DECL_OVERRIDE;

    qreal positionAt(int index) const Q_DECL_OVERRIDE;
    qreal endPositionAt(int index) const Q_DECL_OVERRIDE;
    qreal originPosition() const Q_DECL_OVERRIDE;
    qreal lastPosition() const Q_DECL_OVERRIDE;

    qreal rowSize() const;
    qreal colSize() const;
    qreal colPosAt(int modelIndex) const;
    qreal rowPosAt(int modelIndex) const;
    qreal snapPosAt(qreal pos) const;
    FxViewItem *snapItemAt(qreal pos) const;
    int snapIndex() const;
    qreal contentXForPosition(qreal pos) const;
    qreal contentYForPosition(qreal pos) const;

    void resetColumns();

    bool addVisibleItems(qreal fillFrom, qreal fillTo, qreal bufferFrom, qreal bufferTo, bool doBuffer) Q_DECL_OVERRIDE;
    bool removeNonVisibleItems(qreal bufferFrom, qreal bufferTo) Q_DECL_OVERRIDE;

    void removeItem(FxViewItem *item);

    FxViewItem *newViewItem(int index, QQuickItem *item) Q_DECL_OVERRIDE;
    void initializeViewItem(FxViewItem *item) Q_DECL_OVERRIDE;
    void repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer) Q_DECL_OVERRIDE;
    void repositionPackageItemAt(QQuickItem *item, int index) Q_DECL_OVERRIDE;
    void resetFirstItemPosition(qreal pos = 0.0) Q_DECL_OVERRIDE;
    void adjustFirstItem(qreal forwards, qreal backwards, int changeBeforeVisible) Q_DECL_OVERRIDE;

    void createHighlight() Q_DECL_OVERRIDE;
    void updateHighlight() Q_DECL_OVERRIDE;
    void resetHighlightPosition() Q_DECL_OVERRIDE;

    void setPosition(qreal pos) Q_DECL_OVERRIDE;
    void layoutVisibleItems(int fromModelIndex = 0) Q_DECL_OVERRIDE;
    bool applyInsertionChange(const QQmlChangeSet::Change &insert, ChangeResult *changeResult, QList<FxViewItem *> *addedItems, QList<MovedItem> *movingIntoView) Q_DECL_OVERRIDE;
    void translateAndTransitionItemsAfter(int afterModelIndex, const ChangeResult &insertionResult, const ChangeResult &removalResult) Q_DECL_OVERRIDE;
    bool needsRefillForAddedOrRemovedIndex(int index) const Q_DECL_OVERRIDE;

    qreal headerSize() const Q_DECL_OVERRIDE;
    qreal footerSize() const Q_DECL_OVERRIDE;
    bool showHeaderForIndex(int index) const Q_DECL_OVERRIDE;
    bool showFooterForIndex(int index) const Q_DECL_OVERRIDE;
    void updateHeader() Q_DECL_OVERRIDE;
    void updateFooter() Q_DECL_OVERRIDE;

    void changedVisibleIndex(int newIndex) Q_DECL_OVERRIDE;
    void initializeCurrentItem() Q_DECL_OVERRIDE;

    void updateViewport() Q_DECL_OVERRIDE;
    void fixupPosition() Q_DECL_OVERRIDE;
    void fixup(AxisData &data, qreal minExtent, qreal maxExtent) Q_DECL_OVERRIDE;
    bool flick(QQuickItemViewPrivate::AxisData &data, qreal minExtent, qreal maxExtent, qreal vSize,
               QQuickTimeLineCallback::Callback fixupCallback, qreal velocity) Q_DECL_OVERRIDE;

    QQuickGridView::Flow flow;
    qreal cellWidth;
    qreal cellHeight;
    int columns;
    QQuickGridView::SnapMode snapMode;

    QSmoothedAnimation *highlightXAnimator;
    QSmoothedAnimation *highlightYAnimator;

    QQuickGridViewPrivate()
        : flow(QQuickGridView::FlowLeftToRight)
        , cellWidth(100), cellHeight(100), columns(1)
        , snapMode(QQuickGridView::NoSnap)
        , highlightXAnimator(0), highlightYAnimator(0)
    {}
    ~QQuickGridViewPrivate()
    {
        delete highlightXAnimator;
        delete highlightYAnimator;
    }
};

Qt::Orientation QQuickGridViewPrivate::layoutOrientation() const
{
    return flow == QQuickGridView::FlowLeftToRight ? Qt::Vertical : Qt::Horizontal;
}

bool QQuickGridViewPrivate::isContentFlowReversed() const
{
    Q_Q(const QQuickGridView);

    return (flow == QQuickGridView::FlowLeftToRight && verticalLayoutDirection == QQuickItemView::BottomToTop)
            || (flow == QQuickGridView::FlowTopToBottom && q->effectiveLayoutDirection() == Qt::RightToLeft);
}

void QQuickGridViewPrivate::changedVisibleIndex(int newIndex)
{
    visibleIndex = newIndex / columns * columns;
}

void QQuickGridViewPrivate::setPosition(qreal pos)
{
    Q_Q(QQuickGridView);
    q->QQuickFlickable::setContentX(contentXForPosition(pos));
    q->QQuickFlickable::setContentY(contentYForPosition(pos));
}

qreal QQuickGridViewPrivate::originPosition() const
{
    qreal pos = 0;
    if (!visibleItems.isEmpty())
        pos = static_cast<FxGridItemSG*>(visibleItems.first())->rowPos() - visibleIndex / columns * rowSize();
    return pos;
}

qreal QQuickGridViewPrivate::lastPosition() const
{
    qreal pos = 0;
    if (model && (model->count() || !visibleItems.isEmpty())) {
        qreal lastRowPos = model->count() ? rowPosAt(model->count() - 1) : 0;
        if (!visibleItems.isEmpty()) {
            // If there are items in delayRemove state, they may be after any items linked to the model
            lastRowPos = qMax(lastRowPos, static_cast<FxGridItemSG*>(visibleItems.last())->rowPos());
        }
        pos = lastRowPos + rowSize();
    }
    return pos;
}

qreal QQuickGridViewPrivate::positionAt(int index) const
{
    return rowPosAt(index);
}

qreal QQuickGridViewPrivate::endPositionAt(int index) const
{
    return rowPosAt(index) + rowSize();
}

qreal QQuickGridViewPrivate::rowSize() const {
    return flow == QQuickGridView::FlowLeftToRight ? cellHeight : cellWidth;
}
qreal QQuickGridViewPrivate::colSize() const {
    return flow == QQuickGridView::FlowLeftToRight ? cellWidth : cellHeight;
}

qreal QQuickGridViewPrivate::colPosAt(int modelIndex) const
{
    if (FxViewItem *item = visibleItem(modelIndex))
        return static_cast<FxGridItemSG*>(item)->colPos();
    if (!visibleItems.isEmpty()) {
        if (modelIndex == visibleIndex) {
            FxGridItemSG *firstItem = static_cast<FxGridItemSG*>(visibleItems.first());
            return firstItem->colPos();
        } else if (modelIndex < visibleIndex) {
            int count = (visibleIndex - modelIndex) % columns;
            int col = static_cast<FxGridItemSG*>(visibleItems.first())->colPos() / colSize();
            col = (columns - count + col) % columns;
            return col * colSize();
        } else {
            FxGridItemSG *lastItem = static_cast<FxGridItemSG*>(visibleItems.last());
            int count = modelIndex - lastItem->index;
            int col = lastItem->colPos() / colSize();
            col = (col + count) % columns;
            return col * colSize();
        }
    }
    return (modelIndex % columns) * colSize();
}

qreal QQuickGridViewPrivate::rowPosAt(int modelIndex) const
{
    if (FxViewItem *item = visibleItem(modelIndex))
        return static_cast<FxGridItemSG*>(item)->rowPos();
    if (!visibleItems.isEmpty()) {
        if (modelIndex == visibleIndex) {
            FxGridItemSG *firstItem = static_cast<FxGridItemSG*>(visibleItems.first());
            return firstItem->rowPos();
        } else if (modelIndex < visibleIndex) {
            FxGridItemSG *firstItem = static_cast<FxGridItemSG*>(visibleItems.first());
            int firstCol = firstItem->colPos() / colSize();
            int col = visibleIndex - modelIndex + (columns - firstCol - 1);
            int rows = col / columns;
            return firstItem->rowPos() - rows * rowSize();
        } else {
            FxGridItemSG *lastItem = static_cast<FxGridItemSG*>(visibleItems.last());
            int count = modelIndex - lastItem->index;
            int col = lastItem->colPos() + count * colSize();
            int rows = col / (columns * colSize());
            return lastItem->rowPos() + rows * rowSize();
        }
    }
    return (modelIndex / columns) * rowSize();
}


qreal QQuickGridViewPrivate::snapPosAt(qreal pos) const
{
    Q_Q(const QQuickGridView);
    qreal snapPos = 0;
    if (!visibleItems.isEmpty()) {
        qreal highlightStart = highlightRangeStart;
        pos += highlightStart;
        pos += rowSize()/2;
        snapPos = static_cast<FxGridItemSG*>(visibleItems.first())->rowPos() - visibleIndex / columns * rowSize();
        snapPos = pos - std::fmod(pos - snapPos, qreal(rowSize()));
        snapPos -= highlightStart;
        qreal maxExtent;
        qreal minExtent;
        if (isContentFlowReversed()) {
            maxExtent = q->minXExtent()-size();
            minExtent = q->maxXExtent()-size();
        } else {
            maxExtent = flow == QQuickGridView::FlowLeftToRight ? -q->maxYExtent() : -q->maxXExtent();
            minExtent = flow == QQuickGridView::FlowLeftToRight ? -q->minYExtent() : -q->minXExtent();
        }
        if (snapPos > maxExtent)
            snapPos = maxExtent;
        if (snapPos < minExtent)
            snapPos = minExtent;
    }
    return snapPos;
}

FxViewItem *QQuickGridViewPrivate::snapItemAt(qreal pos) const
{
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == -1)
            continue;
        qreal itemTop = item->position();
        if (itemTop+rowSize()/2 >= pos && itemTop - rowSize()/2 <= pos)
            return item;
    }
    return 0;
}

int QQuickGridViewPrivate::snapIndex() const
{
    int index = currentIndex;
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxGridItemSG *item = static_cast<FxGridItemSG*>(visibleItems.at(i));
        if (item->index == -1)
            continue;
        qreal itemTop = item->position();
        FxGridItemSG *hItem = static_cast<FxGridItemSG*>(highlight);
        if (itemTop >= hItem->rowPos()-rowSize()/2 && itemTop < hItem->rowPos()+rowSize()/2) {
            index = item->index;
            if (item->colPos() >= hItem->colPos()-colSize()/2 && item->colPos() < hItem->colPos()+colSize()/2)
                return item->index;
        }
    }
    return index;
}

qreal QQuickGridViewPrivate::contentXForPosition(qreal pos) const
{
    Q_Q(const QQuickGridView);
    if (flow == QQuickGridView::FlowLeftToRight) {
        // vertical scroll
        if (q->effectiveLayoutDirection() == Qt::LeftToRight) {
            return 0;
        } else {
            qreal colSize = cellWidth;
            int columns = q->width()/colSize;
            return -q->width() + (cellWidth * columns);
        }
    } else {
        // horizontal scroll
        if (q->effectiveLayoutDirection() == Qt::LeftToRight)
            return pos;
        else
            return -pos - q->width();
    }
}

qreal QQuickGridViewPrivate::contentYForPosition(qreal pos) const
{
    Q_Q(const QQuickGridView);
    if (flow == QQuickGridView::FlowLeftToRight) {
        // vertical scroll
        if (verticalLayoutDirection == QQuickItemView::TopToBottom)
            return pos;
        else
            return -pos - q->height();
    } else {
        // horizontal scroll
        if (verticalLayoutDirection == QQuickItemView::TopToBottom)
            return 0;
        else
            return -q->height();
    }
}

void QQuickGridViewPrivate::resetColumns()
{
    Q_Q(QQuickGridView);
    qreal length = flow == QQuickGridView::FlowLeftToRight ? q->width() : q->height();
    columns = qMax(1, qFloor(length / colSize()));
}

FxViewItem *QQuickGridViewPrivate::newViewItem(int modelIndex, QQuickItem *item)
{
    Q_Q(QQuickGridView);
    Q_UNUSED(modelIndex);
    return new FxGridItemSG(item, q, false);
}

void QQuickGridViewPrivate::initializeViewItem(FxViewItem *item)
{
    QQuickItemViewPrivate::initializeViewItem(item);

    // need to track current items that are animating
    item->trackGeometry(true);
}

bool QQuickGridViewPrivate::addVisibleItems(qreal fillFrom, qreal fillTo, qreal bufferFrom, qreal bufferTo, bool doBuffer)
{
    qreal colPos = colPosAt(visibleIndex);
    qreal rowPos = rowPosAt(visibleIndex);
    if (visibleItems.count()) {
        FxGridItemSG *lastItem = static_cast<FxGridItemSG*>(visibleItems.constLast());
        rowPos = lastItem->rowPos();
        int colNum = qFloor((lastItem->colPos()+colSize()/2) / colSize());
        if (++colNum >= columns) {
            colNum = 0;
            rowPos += rowSize();
        }
        colPos = colNum * colSize();
    }

    int modelIndex = findLastVisibleIndex();
    modelIndex = modelIndex < 0 ? visibleIndex : modelIndex + 1;

    if (visibleItems.count() && (bufferFrom > rowPos + rowSize()*2
        || bufferTo < rowPosAt(visibleIndex) - rowSize())) {
        // We've jumped more than a page.  Estimate which items are now
        // visible and fill from there.
        int count = (fillFrom - (rowPos + rowSize())) / (rowSize()) * columns;
        for (int i = 0; i < visibleItems.count(); ++i)
            releaseItem(visibleItems.at(i));
        visibleItems.clear();
        modelIndex += count;
        if (modelIndex >= model->count())
            modelIndex = model->count() - 1;
        else if (modelIndex < 0)
            modelIndex = 0;
        modelIndex = modelIndex / columns * columns;
        visibleIndex = modelIndex;
        colPos = colPosAt(visibleIndex);
        rowPos = rowPosAt(visibleIndex);
    }

    int colNum = qFloor((colPos+colSize()/2) / colSize());
    FxGridItemSG *item = 0;
    bool changed = false;

    while (modelIndex < model->count() && rowPos <= fillTo + rowSize()*(columns - colNum)/(columns+1)) {
        qCDebug(lcItemViewDelegateLifecycle) << "refill: append item" << modelIndex << colPos << rowPos;
        if (!(item = static_cast<FxGridItemSG*>(createItem(modelIndex, doBuffer))))
            break;
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(colPos, rowPos, true);
        QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        visibleItems.append(item);
        if (++colNum >= columns) {
            colNum = 0;
            rowPos += rowSize();
        }
        colPos = colNum * colSize();
        ++modelIndex;
        changed = true;
    }

    if (doBuffer && requestedIndex != -1) // already waiting for an item
        return changed;

    // Find first column
    if (visibleItems.count()) {
        FxGridItemSG *firstItem = static_cast<FxGridItemSG*>(visibleItems.constFirst());
        rowPos = firstItem->rowPos();
        colNum = qFloor((firstItem->colPos()+colSize()/2) / colSize());
        if (--colNum < 0) {
            colNum = columns - 1;
            rowPos -= rowSize();
        }
    } else {
        colNum = qFloor((colPos+colSize()/2) / colSize());
    }

    // Prepend
    colPos = colNum * colSize();
    while (visibleIndex > 0 && rowPos + rowSize() - 1 >= fillFrom - rowSize()*(colNum+1)/(columns+1)){
        qCDebug(lcItemViewDelegateLifecycle) << "refill: prepend item" << visibleIndex-1 << "top pos" << rowPos << colPos;
        if (!(item = static_cast<FxGridItemSG*>(createItem(visibleIndex-1, doBuffer))))
            break;
        --visibleIndex;
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(colPos, rowPos, true);
        QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        visibleItems.prepend(item);
        if (--colNum < 0) {
            colNum = columns-1;
            rowPos -= rowSize();
        }
        colPos = colNum * colSize();
        changed = true;
    }

    return changed;
}

void QQuickGridViewPrivate::removeItem(FxViewItem *item)
{
    if (item->transitionScheduledOrRunning()) {
        qCDebug(lcItemViewDelegateLifecycle) << "\tnot releasing animating item:" << item->index << item->item->objectName();
        item->releaseAfterTransition = true;
        releasePendingTransition.append(item);
    } else {
        releaseItem(item);
    }
}

bool QQuickGridViewPrivate::removeNonVisibleItems(qreal bufferFrom, qreal bufferTo)
{
    FxGridItemSG *item = 0;
    bool changed = false;

    while (visibleItems.count() > 1
           && (item = static_cast<FxGridItemSG*>(visibleItems.constFirst()))
                && item->rowPos()+rowSize()-1 < bufferFrom - rowSize()*(item->colPos()/colSize()+1)/(columns+1)) {
        if (item->attached->delayRemove())
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: remove first" << visibleIndex << "top end pos" << item->endRowPos();
        if (item->index != -1)
            visibleIndex++;
        visibleItems.removeFirst();
        removeItem(item);
        changed = true;
    }
    while (visibleItems.count() > 1
           && (item = static_cast<FxGridItemSG*>(visibleItems.constLast()))
                && item->rowPos() > bufferTo + rowSize()*(columns - item->colPos()/colSize())/(columns+1)) {
        if (item->attached->delayRemove())
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: remove last" << visibleIndex+visibleItems.count()-1;
        visibleItems.removeLast();
        removeItem(item);
        changed = true;
    }

    return changed;
}

void QQuickGridViewPrivate::updateViewport()
{
    resetColumns();
    QQuickItemViewPrivate::updateViewport();
}

void QQuickGridViewPrivate::layoutVisibleItems(int fromModelIndex)
{
    if (visibleItems.count()) {
        const qreal from = isContentFlowReversed() ? -position()-displayMarginBeginning-size() : position()-displayMarginBeginning;
        const qreal to = isContentFlowReversed() ? -position()+displayMarginEnd : position()+size()+displayMarginEnd;

        FxGridItemSG *firstItem = static_cast<FxGridItemSG*>(visibleItems.constFirst());
        qreal rowPos = firstItem->rowPos();
        qreal colPos = firstItem->colPos();
        int col = visibleIndex % columns;
        if (colPos != col * colSize()) {
            colPos = col * colSize();
            firstItem->setPosition(colPos, rowPos);
        }
        firstItem->setVisible(firstItem->rowPos() + rowSize() >= from && firstItem->rowPos() <= to);
        for (int i = 1; i < visibleItems.count(); ++i) {
            FxGridItemSG *item = static_cast<FxGridItemSG*>(visibleItems.at(i));
            if (++col >= columns) {
                col = 0;
                rowPos += rowSize();
            }
            colPos = col * colSize();
            if (item->index >= fromModelIndex) {
                item->setPosition(colPos, rowPos);
                item->setVisible(item->rowPos() + rowSize() >= from && item->rowPos() <= to);
            }
        }
    }
}

void QQuickGridViewPrivate::repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer)
{
    int count = sizeBuffer / rowSize();
    static_cast<FxGridItemSG *>(item)->setPosition(colPosAt(index + count), rowPosAt(index + count));
}

void QQuickGridViewPrivate::repositionPackageItemAt(QQuickItem *item, int index)
{
    Q_Q(QQuickGridView);
    qreal pos = position();
    if (flow == QQuickGridView::FlowLeftToRight) {
        if (item->y() + item->height() > pos && item->y() < pos + q->height()) {
            qreal y = (verticalLayoutDirection == QQuickItemView::TopToBottom)
                    ? rowPosAt(index)
                    : -rowPosAt(index) - item->height();
            item->setPosition(QPointF(colPosAt(index), y));
        }
    } else {
        if (item->x() + item->width() > pos && item->x() < pos + q->width()) {
            qreal y = (verticalLayoutDirection == QQuickItemView::TopToBottom)
                    ? colPosAt(index)
                    : -colPosAt(index) - item->height();
            if (flow == QQuickGridView::FlowTopToBottom && q->effectiveLayoutDirection() == Qt::RightToLeft)
                item->setPosition(QPointF(-rowPosAt(index)-item->width(), y));
            else
                item->setPosition(QPointF(rowPosAt(index), y));
        }
    }
}

void QQuickGridViewPrivate::resetFirstItemPosition(qreal pos)
{
    FxGridItemSG *item = static_cast<FxGridItemSG*>(visibleItems.constFirst());
    item->setPosition(0, pos);
}

void QQuickGridViewPrivate::adjustFirstItem(qreal forwards, qreal backwards, int changeBeforeVisible)
{
    if (!visibleItems.count())
        return;

    int moveCount = (forwards - backwards) / rowSize();
    if (moveCount == 0 && changeBeforeVisible != 0)
        moveCount += (changeBeforeVisible % columns) - (columns - 1);

    FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(visibleItems.constFirst());
    gridItem->setPosition(gridItem->colPos(), gridItem->rowPos() + ((moveCount / columns) * rowSize()));
}

void QQuickGridViewPrivate::createHighlight()
{
    Q_Q(QQuickGridView);
    bool changed = false;
    if (highlight) {
        if (trackedItem == highlight)
            trackedItem = 0;
        delete highlight;
        highlight = 0;

        delete highlightXAnimator;
        delete highlightYAnimator;
        highlightXAnimator = 0;
        highlightYAnimator = 0;

        changed = true;
    }

    if (currentItem) {
        QQuickItem *item = createHighlightItem();
        if (item) {
            FxGridItemSG *newHighlight = new FxGridItemSG(item, q, true);
            newHighlight->trackGeometry(true);
            if (autoHighlight)
                resetHighlightPosition();
            highlightXAnimator = new QSmoothedAnimation;
            highlightXAnimator->target = QQmlProperty(item, QLatin1String("x"));
            highlightXAnimator->userDuration = highlightMoveDuration;
            highlightYAnimator = new QSmoothedAnimation;
            highlightYAnimator->target = QQmlProperty(item, QLatin1String("y"));
            highlightYAnimator->userDuration = highlightMoveDuration;

            highlight = newHighlight;
            changed = true;
        }
    }
    if (changed)
        emit q->highlightItemChanged();
}

void QQuickGridViewPrivate::updateHighlight()
{
    applyPendingChanges();

    if ((!currentItem && highlight) || (currentItem && !highlight))
        createHighlight();
    bool strictHighlight = haveHighlightRange && highlightRange == QQuickGridView::StrictlyEnforceRange;
    if (currentItem && autoHighlight && highlight && (!strictHighlight || !pressed)) {
        // auto-update highlight
        highlightXAnimator->to = currentItem->itemX();
        highlightYAnimator->to = currentItem->itemY();
        highlight->item->setWidth(currentItem->item->width());
        highlight->item->setHeight(currentItem->item->height());

        highlightXAnimator->restart();
        highlightYAnimator->restart();
    }
    updateTrackedItem();
}

void QQuickGridViewPrivate::resetHighlightPosition()
{
    if (highlight && currentItem) {
        FxGridItemSG *cItem = static_cast<FxGridItemSG*>(currentItem);
        static_cast<FxGridItemSG*>(highlight)->setPosition(cItem->colPos(), cItem->rowPos());
    }
}

qreal QQuickGridViewPrivate::headerSize() const
{
    if (!header)
        return 0.0;
    return flow == QQuickGridView::FlowLeftToRight ? header->item->height() : header->item->width();
}

qreal QQuickGridViewPrivate::footerSize() const
{
    if (!footer)
        return 0.0;
    return flow == QQuickGridView::FlowLeftToRight? footer->item->height() : footer->item->width();
}

bool QQuickGridViewPrivate::showHeaderForIndex(int index) const
{
    return index / columns == 0;
}

bool QQuickGridViewPrivate::showFooterForIndex(int index) const
{
    return index / columns == (model->count()-1) / columns;
}

void QQuickGridViewPrivate::updateFooter()
{
    Q_Q(QQuickGridView);
    bool created = false;
    if (!footer) {
        QQuickItem *item = createComponentItem(footerComponent, 1.0);
        if (!item)
            return;
        footer = new FxGridItemSG(item, q, true);
        footer->trackGeometry(true);
        created = true;
    }

    FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(footer);
    qreal colOffset = 0;
    qreal rowOffset = 0;
    if (q->effectiveLayoutDirection() == Qt::RightToLeft) {
        if (flow == QQuickGridView::FlowTopToBottom)
            rowOffset += gridItem->item->width() - cellWidth;
        else
            colOffset += gridItem->item->width() - cellWidth;
    }
    if (verticalLayoutDirection == QQuickItemView::BottomToTop) {
        if (flow == QQuickGridView::FlowTopToBottom)
            colOffset += gridItem->item->height() - cellHeight;
        else
            rowOffset += gridItem->item->height() - cellHeight;
    }
    if (visibleItems.count()) {
        qreal endPos = lastPosition();
        if (findLastVisibleIndex() == model->count()-1) {
            gridItem->setPosition(colOffset, endPos + rowOffset);
        } else {
            qreal visiblePos = isContentFlowReversed() ? -position() : position() + size();
            if (endPos <= visiblePos || gridItem->endPosition() <= endPos + rowOffset)
                gridItem->setPosition(colOffset, endPos + rowOffset);
        }
    } else {
        gridItem->setPosition(colOffset, rowOffset);
    }

    if (created)
        emit q->footerItemChanged();
}

void QQuickGridViewPrivate::updateHeader()
{
    Q_Q(QQuickGridView);
    bool created = false;
    if (!header) {
        QQuickItem *item = createComponentItem(headerComponent, 1.0);
        if (!item)
            return;
        header = new FxGridItemSG(item, q, true);
        header->trackGeometry(true);
        created = true;
    }

    FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(header);
    qreal colOffset = 0;
    qreal rowOffset = -headerSize();
    if (q->effectiveLayoutDirection() == Qt::RightToLeft) {
        if (flow == QQuickGridView::FlowTopToBottom)
            rowOffset += gridItem->item->width() - cellWidth;
        else
            colOffset += gridItem->item->width() - cellWidth;
    }
    if (verticalLayoutDirection == QQuickItemView::BottomToTop) {
        if (flow == QQuickGridView::FlowTopToBottom)
            colOffset += gridItem->item->height() - cellHeight;
        else
            rowOffset += gridItem->item->height() - cellHeight;
    }
    if (visibleItems.count()) {
        qreal startPos = originPosition();
        if (visibleIndex == 0) {
            gridItem->setPosition(colOffset, startPos + rowOffset);
        } else {
            qreal tempPos = isContentFlowReversed() ? -position()-size() : position();
            qreal headerPos = isContentFlowReversed() ? gridItem->rowPos() + cellWidth - headerSize() : gridItem->rowPos();
            if (tempPos <= startPos || headerPos > startPos + rowOffset)
                gridItem->setPosition(colOffset, startPos + rowOffset);
        }
    } else {
        if (isContentFlowReversed())
            gridItem->setPosition(colOffset, rowOffset);
        else
            gridItem->setPosition(colOffset, -headerSize());
    }

    if (created)
        emit q->headerItemChanged();
}

void QQuickGridViewPrivate::initializeCurrentItem()
{
    if (currentItem && currentIndex >= 0) {
        FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(currentItem);
        FxViewItem *actualItem = visibleItem(currentIndex);

        // don't reposition the item if it's about to be transitioned to another position
        if ((!actualItem || !actualItem->transitionScheduledOrRunning()))
            gridItem->setPosition(colPosAt(currentIndex), rowPosAt(currentIndex));
    }
}

void QQuickGridViewPrivate::fixupPosition()
{
    moveReason = Other;
    if (flow == QQuickGridView::FlowLeftToRight)
        fixupY();
    else
        fixupX();
}

void QQuickGridViewPrivate::fixup(AxisData &data, qreal minExtent, qreal maxExtent)
{
    if ((flow == QQuickGridView::FlowTopToBottom && &data == &vData)
        || (flow == QQuickGridView::FlowLeftToRight && &data == &hData))
        return;

    fixupMode = moveReason == Mouse ? fixupMode : Immediate;

    qreal viewPos = isContentFlowReversed() ? -position()-size() : position();

    bool strictHighlightRange = haveHighlightRange && highlightRange == QQuickGridView::StrictlyEnforceRange;
    if (snapMode != QQuickGridView::NoSnap) {
        qreal tempPosition = isContentFlowReversed() ? -position()-size() : position();
        if (snapMode == QQuickGridView::SnapOneRow && moveReason == Mouse) {
            // if we've been dragged < rowSize()/2 then bias towards the next row
            qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
            qreal bias = 0;
            if (data.velocity > 0 && dist > QML_FLICK_SNAPONETHRESHOLD && dist < rowSize()/2)
                bias = rowSize()/2;
            else if (data.velocity < 0 && dist < -QML_FLICK_SNAPONETHRESHOLD && dist > -rowSize()/2)
                bias = -rowSize()/2;
            if (isContentFlowReversed())
                bias = -bias;
            tempPosition -= bias;
        }
        FxViewItem *topItem = snapItemAt(tempPosition+highlightRangeStart);
        if (strictHighlightRange && currentItem && (!topItem || topItem->index != currentIndex)) {
            // StrictlyEnforceRange always keeps an item in range
            updateHighlight();
            topItem = currentItem;
        }
        FxViewItem *bottomItem = snapItemAt(tempPosition+highlightRangeEnd);
        if (strictHighlightRange && currentItem && (!bottomItem || bottomItem->index != currentIndex)) {
            // StrictlyEnforceRange always keeps an item in range
            updateHighlight();
            bottomItem = currentItem;
        }
        qreal pos;
        bool isInBounds = -position() > maxExtent && -position() <= minExtent;
        if (topItem && (isInBounds || strictHighlightRange)) {
            qreal headerPos = header ? static_cast<FxGridItemSG*>(header)->rowPos() : 0;
            if (topItem->index == 0 && header && tempPosition+highlightRangeStart < headerPos+headerSize()/2 && !strictHighlightRange) {
                pos = isContentFlowReversed() ? - headerPos + highlightRangeStart - size() : headerPos - highlightRangeStart;
            } else {
                if (isContentFlowReversed())
                    pos = qMax(qMin(-topItem->position() + highlightRangeStart - size(), -maxExtent), -minExtent);
                else
                    pos = qMax(qMin(topItem->position() - highlightRangeStart, -maxExtent), -minExtent);
            }
        } else if (bottomItem && isInBounds) {
            if (isContentFlowReversed())
                pos = qMax(qMin(-bottomItem->position() + highlightRangeEnd - size(), -maxExtent), -minExtent);
            else
                pos = qMax(qMin(bottomItem->position() - highlightRangeEnd, -maxExtent), -minExtent);
        } else {
            QQuickItemViewPrivate::fixup(data, minExtent, maxExtent);
            return;
        }

        qreal dist = qAbs(data.move + pos);
        if (dist > 0) {
            timeline.reset(data.move);
            if (fixupMode != Immediate) {
                timeline.move(data.move, -pos, QEasingCurve(QEasingCurve::InOutQuad), fixupDuration/2);
                data.fixingUp = true;
            } else {
                timeline.set(data.move, -pos);
            }
            vTime = timeline.time();
        }
    } else if (haveHighlightRange && highlightRange == QQuickGridView::StrictlyEnforceRange) {
        if (currentItem) {
            updateHighlight();
            qreal pos = static_cast<FxGridItemSG*>(currentItem)->rowPos();
            if (viewPos < pos + rowSize() - highlightRangeEnd)
                viewPos = pos + rowSize() - highlightRangeEnd;
            if (viewPos > pos - highlightRangeStart)
                viewPos = pos - highlightRangeStart;
            if (isContentFlowReversed())
                viewPos = -viewPos-size();
            timeline.reset(data.move);
            if (viewPos != position()) {
                if (fixupMode != Immediate) {
                    timeline.move(data.move, -viewPos, QEasingCurve(QEasingCurve::InOutQuad), fixupDuration/2);
                    data.fixingUp = true;
                } else {
                    timeline.set(data.move, -viewPos);
                }
            }
            vTime = timeline.time();
        }
    } else {
        QQuickItemViewPrivate::fixup(data, minExtent, maxExtent);
    }
    data.inOvershoot = false;
    fixupMode = Normal;
}

bool QQuickGridViewPrivate::flick(AxisData &data, qreal minExtent, qreal maxExtent, qreal vSize,
                                        QQuickTimeLineCallback::Callback fixupCallback, qreal velocity)
{
    data.fixingUp = false;
    moveReason = Mouse;
    if ((!haveHighlightRange || highlightRange != QQuickGridView::StrictlyEnforceRange)
        && snapMode == QQuickGridView::NoSnap) {
        return QQuickItemViewPrivate::flick(data, minExtent, maxExtent, vSize, fixupCallback, velocity);
    }
    qreal maxDistance = 0;
    qreal dataValue = isContentFlowReversed() ? -data.move.value()+size() : data.move.value();
    // -ve velocity means list is moving up/left
    if (velocity > 0) {
        if (data.move.value() < minExtent) {
            if (snapMode == QQuickGridView::SnapOneRow) {
                // if we've been dragged < averageSize/2 then bias towards the next item
                qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
                qreal bias = dist < rowSize()/2 ? rowSize()/2 : 0;
                if (isContentFlowReversed())
                    bias = -bias;
                data.flickTarget = -snapPosAt(-dataValue - bias);
                maxDistance = qAbs(data.flickTarget - data.move.value());
                velocity = maxVelocity;
            } else {
                maxDistance = qAbs(minExtent - data.move.value());
            }
        }
        if (snapMode == QQuickGridView::NoSnap && highlightRange != QQuickGridView::StrictlyEnforceRange)
            data.flickTarget = minExtent;
    } else {
        if (data.move.value() > maxExtent) {
            if (snapMode == QQuickGridView::SnapOneRow) {
                // if we've been dragged < averageSize/2 then bias towards the next item
                qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
                qreal bias = -dist < rowSize()/2 ? rowSize()/2 : 0;
                if (isContentFlowReversed())
                    bias = -bias;
                data.flickTarget = -snapPosAt(-dataValue + bias);
                maxDistance = qAbs(data.flickTarget - data.move.value());
                velocity = -maxVelocity;
            } else {
                maxDistance = qAbs(maxExtent - data.move.value());
            }
        }
        if (snapMode == QQuickGridView::NoSnap && highlightRange != QQuickGridView::StrictlyEnforceRange)
            data.flickTarget = maxExtent;
    }
    bool overShoot = boundsBehavior & QQuickFlickable::OvershootBounds;
    if (maxDistance > 0 || overShoot) {
        // This mode requires the grid to stop exactly on a row boundary.
        qreal v = velocity;
        if (maxVelocity != -1 && maxVelocity < qAbs(v)) {
            if (v < 0)
                v = -maxVelocity;
            else
                v = maxVelocity;
        }
        qreal accel = deceleration;
        qreal v2 = v * v;
        qreal overshootDist = 0.0;
        if ((maxDistance > 0.0 && v2 / (2.0f * maxDistance) < accel) || snapMode == QQuickGridView::SnapOneRow) {
            // + rowSize()/4 to encourage moving at least one item in the flick direction
            qreal dist = v2 / (accel * 2.0) + rowSize()/4;
            dist = qMin(dist, maxDistance);
            if (v > 0)
                dist = -dist;
            if (snapMode != QQuickGridView::SnapOneRow) {
                qreal distTemp = isContentFlowReversed() ? -dist : dist;
                data.flickTarget = -snapPosAt(-dataValue + distTemp);
            }
            data.flickTarget = isContentFlowReversed() ? -data.flickTarget+size() : data.flickTarget;
            if (overShoot) {
                if (data.flickTarget >= minExtent) {
                    overshootDist = overShootDistance(vSize);
                    data.flickTarget += overshootDist;
                } else if (data.flickTarget <= maxExtent) {
                    overshootDist = overShootDistance(vSize);
                    data.flickTarget -= overshootDist;
                }
            }
            qreal adjDist = -data.flickTarget + data.move.value();
            if (qAbs(adjDist) > qAbs(dist)) {
                // Prevent painfully slow flicking - adjust velocity to suit flickDeceleration
                qreal adjv2 = accel * 2.0f * qAbs(adjDist);
                if (adjv2 > v2) {
                    v2 = adjv2;
                    v = qSqrt(v2);
                    if (dist > 0)
                        v = -v;
                }
            }
            dist = adjDist;
            accel = v2 / (2.0f * qAbs(dist));
        } else {
            data.flickTarget = velocity > 0 ? minExtent : maxExtent;
            overshootDist = overShoot ? overShootDistance(vSize) : 0;
        }
        timeline.reset(data.move);
        timeline.accel(data.move, v, accel, maxDistance + overshootDist);
        timeline.callback(QQuickTimeLineCallback(&data.move, fixupCallback, this));
        return true;
    } else {
        timeline.reset(data.move);
        fixup(data, minExtent, maxExtent);
        return false;
    }
}


//----------------------------------------------------------------------------
/*!
    \qmltype GridView
    \instantiates QQuickGridView
    \inqmlmodule QtQuick
    \ingroup qtquick-views

    \inherits Flickable
    \brief For specifying a grid view of items provided by a model

    A GridView displays data from models created from built-in QML types like ListModel
    and XmlListModel, or custom model classes defined in C++ that inherit from
    QAbstractListModel.

    A GridView has a \l model, which defines the data to be displayed, and
    a \l delegate, which defines how the data should be displayed. Items in a
    GridView are laid out horizontally or vertically. Grid views are inherently flickable
    as GridView inherits from \l Flickable.

    \section1 Example Usage

    The following example shows the definition of a simple list model defined
    in a file called \c ContactModel.qml:

    \snippet qml/gridview/ContactModel.qml 0

    \div {class="float-right"}
    \inlineimage gridview-simple.png
    \enddiv

    This model can be referenced as \c ContactModel in other QML files. See \l{QML Modules}
    for more information about creating reusable components like this.

    Another component can display this model data in a GridView, as in the following
    example, which creates a \c ContactModel component for its model, and a \l Column
    (containing \l Image and \l Text items) for its delegate.

    \clearfloat
    \snippet qml/gridview/gridview.qml import
    \codeline
    \snippet qml/gridview/gridview.qml classdocs simple

    \div {class="float-right"}
    \inlineimage gridview-highlight.png
    \enddiv

    The view will create a new delegate for each item in the model. Note that the delegate
    is able to access the model's \c name and \c portrait data directly.

    An improved grid view is shown below. The delegate is visually improved and is moved
    into a separate \c contactDelegate component.

    \clearfloat
    \snippet qml/gridview/gridview.qml classdocs advanced

    The currently selected item is highlighted with a blue \l Rectangle using the \l highlight property,
    and \c focus is set to \c true to enable keyboard navigation for the grid view.
    The grid view itself is a focus scope (see \l{Keyboard Focus in Qt Quick} for more details).

    Delegates are instantiated as needed and may be destroyed at any time.
    State should \e never be stored in a delegate.

    GridView attaches a number of properties to the root item of the delegate, for example
    \c {GridView.isCurrentItem}.  In the following example, the root delegate item can access
    this attached property directly as \c GridView.isCurrentItem, while the child
    \c contactInfo object must refer to this property as \c wrapper.GridView.isCurrentItem.

    \snippet qml/gridview/gridview.qml isCurrentItem

    \note Views do not set the \l{Item::}{clip} property automatically.
    If the view is not clipped by another item or the screen, it will be necessary
    to set this property to true in order to clip the items that are partially or
    fully outside the view.


    \section1 GridView Layouts

    The layout of the items in a GridView can be controlled by these properties:

    \list
    \li \l flow - controls whether items flow from left to right (as a series of rows)
        or from top to bottom (as a series of columns). This value can be either
        GridView.FlowLeftToRight or GridView.FlowTopToBottom.
    \li \l layoutDirection - controls the horizontal layout direction: that is, whether items
        are laid out from the left side of the view to the right, or vice-versa. This value can
        be either Qt.LeftToRight or Qt.RightToLeft.
    \li \l verticalLayoutDirection - controls the vertical layout direction: that is, whether items
        are laid out from the top of the view down towards the bottom of the view, or vice-versa.
        This value can be either GridView.TopToBottom or GridView.BottomToTop.
    \endlist

    By default, a GridView flows from left to right, and items are laid out from left to right
    horizontally, and from top to bottom vertically.

    These properties can be combined to produce a variety of layouts, as shown in the table below.
    The GridViews in the first row all have a \l flow value of GridView.FlowLeftToRight, but use
    different combinations of horizontal and vertical layout directions (specified by \l layoutDirection
    and \l verticalLayoutDirection respectively). Similarly, the GridViews in the second row below
    all have a \l flow value of GridView.FlowTopToBottom, but use different combinations of horizontal and
    vertical layout directions to lay out their items in different ways.

    \table
    \header
        \li {4, 1}
            \b GridViews with GridView.FlowLeftToRight flow
    \row
        \li \b (H) Left to right \b (V) Top to bottom
            \image gridview-layout-lefttoright-ltr-ttb.png
        \li \b (H) Right to left \b (V) Top to bottom
            \image gridview-layout-lefttoright-rtl-ttb.png
        \li \b (H) Left to right \b (V) Bottom to top
            \image gridview-layout-lefttoright-ltr-btt.png
        \li \b (H) Right to left \b (V) Bottom to top
            \image gridview-layout-lefttoright-rtl-btt.png
    \header
        \li {4, 1}
            \b GridViews with GridView.FlowTopToBottom flow
    \row
        \li \b (H) Left to right \b (V) Top to bottom
            \image gridview-layout-toptobottom-ltr-ttb.png
        \li \b (H) Right to left \b (V) Top to bottom
            \image gridview-layout-toptobottom-rtl-ttb.png
        \li \b (H) Left to right \b (V) Bottom to top
            \image gridview-layout-toptobottom-ltr-btt.png
        \li \b (H) Right to left \b (V) Bottom to top
            \image gridview-layout-toptobottom-rtl-btt.png
    \endtable

    \sa {QML Data Models}, ListView, PathView, {Qt Quick Examples - Views}
*/

QQuickGridView::QQuickGridView(QQuickItem *parent)
    : QQuickItemView(*(new QQuickGridViewPrivate), parent)
{
}

QQuickGridView::~QQuickGridView()
{
}

void QQuickGridView::setHighlightFollowsCurrentItem(bool autoHighlight)
{
    Q_D(QQuickGridView);
    if (d->autoHighlight != autoHighlight) {
        if (!autoHighlight && d->highlightXAnimator) {
            d->highlightXAnimator->stop();
            d->highlightYAnimator->stop();
        }
        QQuickItemView::setHighlightFollowsCurrentItem(autoHighlight);
    }
}

/*!
    \qmlattachedproperty bool QtQuick::GridView::isCurrentItem
    This attached property is true if this delegate is the current item; otherwise false.

    It is attached to each instance of the delegate.
*/

/*!
    \qmlattachedproperty GridView QtQuick::GridView::view
    This attached property holds the view that manages this delegate instance.

    It is attached to each instance of the delegate and also to the header, the footer
    and the highlight delegates.

    \snippet qml/gridview/gridview.qml isCurrentItem
*/

/*!
    \qmlattachedproperty bool QtQuick::GridView::delayRemove
    This attached property holds whether the delegate may be destroyed. It
    is attached to each instance of the delegate. The default value is false.

    It is sometimes necessary to delay the destruction of an item
    until an animation completes. The example delegate below ensures that the
    animation completes before the item is removed from the list.

    \snippet qml/gridview/gridview.qml delayRemove

    If a \l remove transition has been specified, it will not be applied until
    delayRemove is returned to \c false.
*/

/*!
    \qmlattachedsignal QtQuick::GridView::add()
    This attached signal is emitted immediately after an item is added to the view.

    The corresponding handler is \c onAdd.
*/

/*!
    \qmlattachedsignal QtQuick::GridView::remove()
    This attached signal is emitted immediately before an item is removed from the view.

    If a \l remove transition has been specified, it is applied after
    this signal is handled, providing that \l delayRemove is false.

    The corresponding handler is \c onRemove.
*/


/*!
  \qmlproperty model QtQuick::GridView::model
  This property holds the model providing data for the grid.

    The model provides the set of data that is used to create the items
    in the view. Models can be created directly in QML using \l ListModel, \l XmlListModel
    or \l VisualItemModel, or provided by C++ model classes. If a C++ model class is
    used, it must be a subclass of \l QAbstractItemModel or a simple list.

  \sa {qml-data-models}{Data Models}
*/

/*!
    \qmlproperty Component QtQuick::GridView::delegate

    The delegate provides a template defining each item instantiated by the view.
    The index is exposed as an accessible \c index property.  Properties of the
    model are also available depending upon the type of \l {qml-data-models}{Data Model}.

    The number of objects and bindings in the delegate has a direct effect on the
    flicking performance of the view.  If at all possible, place functionality
    that is not needed for the normal display of the delegate in a \l Loader which
    can load additional components when needed.

    The item size of the GridView is determined by cellHeight and cellWidth. It will not resize the items
    based on the size of the root item in the delegate.

    The default \l {QQuickItem::z}{stacking order} of delegate instances is \c 1.

    \note Delegates are instantiated as needed and may be destroyed at any time.
    State should \e never be stored in a delegate.
*/

/*!
  \qmlproperty int QtQuick::GridView::currentIndex
  \qmlproperty Item QtQuick::GridView::currentItem

    The \c currentIndex property holds the index of the current item, and
    \c currentItem holds the current item.  Setting the currentIndex to -1
    will clear the highlight and set currentItem to null.

    If highlightFollowsCurrentItem is \c true, setting either of these
    properties will smoothly scroll the GridView so that the current
    item becomes visible.

    Note that the position of the current item
    may only be approximate until it becomes visible in the view.
*/


/*!
  \qmlproperty Item QtQuick::GridView::highlightItem

  This holds the highlight item created from the \l highlight component.

  The highlightItem is managed by the view unless
  \l highlightFollowsCurrentItem is set to false.
  The default \l {QQuickItem::z}{stacking order}
  of the highlight item is \c 0.

  \sa highlight, highlightFollowsCurrentItem
*/


/*!
  \qmlproperty int QtQuick::GridView::count
  This property holds the number of items in the view.
*/


/*!
  \qmlproperty Component QtQuick::GridView::highlight
  This property holds the component to use as the highlight.

  An instance of the highlight component is created for each view.
  The geometry of the resulting component instance will be managed by the view
  so as to stay with the current item, unless the highlightFollowsCurrentItem property is false.
  The default \l {QQuickItem::z}{stacking order} of the highlight item is \c 0.

  \sa highlightItem, highlightFollowsCurrentItem
*/

/*!
  \qmlproperty bool QtQuick::GridView::highlightFollowsCurrentItem
  This property sets whether the highlight is managed by the view.

    If this property is true (the default value), the highlight is moved smoothly
    to follow the current item.  Otherwise, the
    highlight is not moved by the view, and any movement must be implemented
    by the highlight.

    Here is a highlight with its motion defined by a \l {SpringAnimation} item:

    \snippet qml/gridview/gridview.qml highlightFollowsCurrentItem
*/


/*!
    \qmlproperty int QtQuick::GridView::highlightMoveDuration
    This property holds the move animation duration of the highlight delegate.

    highlightFollowsCurrentItem must be true for this property
    to have effect.

    The default value for the duration is 150ms.

    \sa highlightFollowsCurrentItem
*/

/*!
    \qmlproperty real QtQuick::GridView::preferredHighlightBegin
    \qmlproperty real QtQuick::GridView::preferredHighlightEnd
    \qmlproperty enumeration QtQuick::GridView::highlightRangeMode

    These properties define the preferred range of the highlight (for the current item)
    within the view. The \c preferredHighlightBegin value must be less than the
    \c preferredHighlightEnd value.

    These properties affect the position of the current item when the view is scrolled.
    For example, if the currently selected item should stay in the middle of the
    view when it is scrolled, set the \c preferredHighlightBegin and
    \c preferredHighlightEnd values to the top and bottom coordinates of where the middle
    item would be. If the \c currentItem is changed programmatically, the view will
    automatically scroll so that the current item is in the middle of the view.
    Furthermore, the behavior of the current item index will occur whether or not a
    highlight exists.

    Valid values for \c highlightRangeMode are:

    \list
    \li GridView.ApplyRange - the view attempts to maintain the highlight within the range.
       However, the highlight can move outside of the range at the ends of the view or due
       to mouse interaction.
    \li GridView.StrictlyEnforceRange - the highlight never moves outside of the range.
       The current item changes if a keyboard or mouse action would cause the highlight to move
       outside of the range.
    \li GridView.NoHighlightRange - this is the default value.
    \endlist
*/


/*!
  \qmlproperty enumeration QtQuick::GridView::layoutDirection
  This property holds the layout direction of the grid.

    Possible values:

  \list
  \li Qt.LeftToRight (default) - Items will be laid out starting in the top, left corner. The flow is
  dependent on the \l GridView::flow property.
  \li Qt.RightToLeft - Items will be laid out starting in the top, right corner. The flow is dependent
  on the \l GridView::flow property.
  \endlist

  \b Note: If GridView::flow is set to GridView.FlowLeftToRight, this is not to be confused if
  GridView::layoutDirection is set to Qt.RightToLeft. The GridView.FlowLeftToRight flow value simply
  indicates that the flow is horizontal.

  \sa GridView::effectiveLayoutDirection, GridView::verticalLayoutDirection
*/


/*!
    \qmlproperty enumeration QtQuick::GridView::effectiveLayoutDirection
    This property holds the effective layout direction of the grid.

    When using the attached property \l {LayoutMirroring::enabled}{LayoutMirroring::enabled} for locale layouts,
    the visual layout direction of the grid will be mirrored. However, the
    property \l {GridView::layoutDirection}{layoutDirection} will remain unchanged.

    \sa GridView::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/

/*!
  \qmlproperty enumeration QtQuick::GridView::verticalLayoutDirection
  This property holds the vertical layout direction of the grid.

  Possible values:

  \list
  \li GridView.TopToBottom (default) - Items are laid out from the top of the view down to the bottom of the view.
  \li GridView.BottomToTop - Items are laid out from the bottom of the view up to the top of the view.
  \endlist

  \sa GridView::layoutDirection
*/

/*!
  \qmlproperty bool QtQuick::GridView::keyNavigationWraps
  This property holds whether the grid wraps key navigation

    If this is true, key navigation that would move the current item selection
    past one end of the view instead wraps around and moves the selection to
    the other end of the view.

    By default, key navigation is not wrapped.
*/

/*!
    \qmlproperty bool QtQuick::GridView::keyNavigationEnabled
    \since 5.7

    This property holds whether the key navigation of the grid is enabled.

    If this is \c true, the user can navigate the view with a keyboard.
    It is useful for applications that need to selectively enable or
    disable mouse and keyboard interaction.

    By default, the value of this property is bound to
    \l {Flickable::}{interactive} to ensure behavior compatibility for
    existing applications. When explicitly set, it will cease to be bound to
    the interactive property.

    \sa {Flickable::}{interactive}
*/

/*!
    \qmlproperty int QtQuick::GridView::cacheBuffer
    This property determines whether delegates are retained outside the
    visible area of the view.

    If this value is greater than zero, the view may keep as many delegates
    instantiated as will fit within the buffer specified.  For example,
    if in a vertical view the delegate is 20 pixels high, there are 3
    columns and \c cacheBuffer is
    set to 40, then up to 6 delegates above and 6 delegates below the visible
    area may be created/retained.  The buffered delegates are created asynchronously,
    allowing creation to occur across multiple frames and reducing the
    likelihood of skipping frames.  In order to improve painting performance
    delegates outside the visible area are not painted.

    The default value of this property is platform dependent, but will usually
    be a value greater than zero. Negative values are ignored.

    Note that cacheBuffer is not a pixel buffer - it only maintains additional
    instantiated delegates.

    \note Setting this property is not a replacement for creating efficient delegates.
    It can improve the smoothness of scrolling behavior at the expense of additional
    memory usage. The fewer objects and bindings in a delegate, the faster a
    view can be scrolled. It is important to realize that setting a cacheBuffer
    will only postpone issues caused by slow-loading delegates, it is not a
    solution for this scenario.

    The cacheBuffer operates outside of any display margins specified by
    displayMarginBeginning or displayMarginEnd.
*/

/*!
    \qmlproperty int QtQuick::GridView::displayMarginBeginning
    \qmlproperty int QtQuick::GridView::displayMarginEnd
    \since QtQuick 2.3

    This property allows delegates to be displayed outside of the view geometry.

    If this value is non-zero, the view will create extra delegates before the
    start of the view, or after the end. The view will create as many delegates
    as it can fit into the pixel size specified.

    For example, if in a vertical view the delegate is 20 pixels high,
    there are 3 columns, and
    \c displayMarginBeginning and \c displayMarginEnd are both set to 40,
    then 6 delegates above and 6 delegates below will be created and shown.

    The default value is 0.

    This property is meant for allowing certain UI configurations,
    and not as a performance optimization. If you wish to create delegates
    outside of the view geometry for performance reasons, you probably
    want to use the cacheBuffer property instead.
*/

void QQuickGridView::setHighlightMoveDuration(int duration)
{
    Q_D(QQuickGridView);
    if (d->highlightMoveDuration != duration) {
        if (d->highlightYAnimator) {
            d->highlightXAnimator->userDuration = duration;
            d->highlightYAnimator->userDuration = duration;
        }
        QQuickItemView::setHighlightMoveDuration(duration);
    }
}

/*!
  \qmlproperty enumeration QtQuick::GridView::flow
  This property holds the flow of the grid.

    Possible values:

    \list
    \li GridView.FlowLeftToRight (default) - Items are laid out from left to right, and the view scrolls vertically
    \li GridView.FlowTopToBottom - Items are laid out from top to bottom, and the view scrolls horizontally
    \endlist
*/
QQuickGridView::Flow QQuickGridView::flow() const
{
    Q_D(const QQuickGridView);
    return d->flow;
}

void QQuickGridView::setFlow(Flow flow)
{
    Q_D(QQuickGridView);
    if (d->flow != flow) {
        d->flow = flow;
        if (d->flow == FlowLeftToRight) {
            setContentWidth(-1);
            setFlickableDirection(VerticalFlick);
        } else {
            setContentHeight(-1);
            setFlickableDirection(HorizontalFlick);
        }
        setContentX(0);
        setContentY(0);
        d->regenerate(true);
        emit flowChanged();
    }
}


/*!
  \qmlproperty real QtQuick::GridView::cellWidth
  \qmlproperty real QtQuick::GridView::cellHeight

  These properties holds the width and height of each cell in the grid.

  The default cell size is 100x100.
*/
qreal QQuickGridView::cellWidth() const
{
    Q_D(const QQuickGridView);
    return d->cellWidth;
}

void QQuickGridView::setCellWidth(qreal cellWidth)
{
    Q_D(QQuickGridView);
    if (cellWidth != d->cellWidth && cellWidth > 0) {
        d->cellWidth = qMax(qreal(1), cellWidth);
        d->updateViewport();
        emit cellWidthChanged();
        d->forceLayoutPolish();
    }
}

qreal QQuickGridView::cellHeight() const
{
    Q_D(const QQuickGridView);
    return d->cellHeight;
}

void QQuickGridView::setCellHeight(qreal cellHeight)
{
    Q_D(QQuickGridView);
    if (cellHeight != d->cellHeight && cellHeight > 0) {
        d->cellHeight = qMax(qreal(1), cellHeight);
        d->updateViewport();
        emit cellHeightChanged();
        d->forceLayoutPolish();
    }
}
/*!
    \qmlproperty enumeration QtQuick::GridView::snapMode

    This property determines how the view scrolling will settle following a drag or flick.
    The possible values are:

    \list
    \li GridView.NoSnap (default) - the view stops anywhere within the visible area.
    \li GridView.SnapToRow - the view settles with a row (or column for \c GridView.FlowTopToBottom flow)
    aligned with the start of the view.
    \li GridView.SnapOneRow - the view will settle no more than one row (or column for \c GridView.FlowTopToBottom flow)
    away from the first visible row at the time the mouse button is released.
    This mode is particularly useful for moving one page at a time.
    \endlist

*/
QQuickGridView::SnapMode QQuickGridView::snapMode() const
{
    Q_D(const QQuickGridView);
    return d->snapMode;
}

void QQuickGridView::setSnapMode(SnapMode mode)
{
    Q_D(QQuickGridView);
    if (d->snapMode != mode) {
        d->snapMode = mode;
        emit snapModeChanged();
    }
}


/*!
    \qmlproperty Component QtQuick::GridView::footer
    This property holds the component to use as the footer.

    An instance of the footer component is created for each view.  The
    footer is positioned at the end of the view, after any items. The
    default \l {QQuickItem::z}{stacking order} of the footer is \c 1.

    \sa header, footerItem
*/
/*!
    \qmlproperty Component QtQuick::GridView::header
    This property holds the component to use as the header.

    An instance of the header component is created for each view.  The
    header is positioned at the beginning of the view, before any items.
    The default \l {QQuickItem::z}{stacking order} of the header is \c 1.

    \sa footer, headerItem
*/

/*!
    \qmlproperty Item QtQuick::GridView::headerItem
    This holds the header item created from the \l header component.

    An instance of the header component is created for each view.  The
    header is positioned at the beginning of the view, before any items.
    The default \l {QQuickItem::z}{stacking order} of the header is \c 1.

    \sa header, footerItem
*/

/*!
    \qmlproperty Item QtQuick::GridView::footerItem
    This holds the footer item created from the \l footer component.

    An instance of the footer component is created for each view.  The
    footer is positioned at the end of the view, after any items. The
    default \l {QQuickItem::z}{stacking order} of the footer is \c 1.

    \sa footer, headerItem
*/

/*!
    \qmlproperty Transition QtQuick::GridView::populate

    This property holds the transition to apply to the items that are initially created
    for a view.

    It is applied to all items that are created when:

    \list
    \li The view is first created
    \li The view's \l model changes
    \li The view's \l model is \l {QAbstractItemModel::reset()}{reset}, if the model is a QAbstractItemModel subclass
    \endlist

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        populate: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    When the view is initialized, the view will create all the necessary items for the view,
    then animate them to their correct positions within the view over one second.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa add, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::add

    This property holds the transition to apply to items that are added to the view.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        add: Transition {
            NumberAnimation { properties: "x,y"; from: 100; duration: 1000 }
        }
    }
    \endcode

    Whenever an item is added to the above view, the item will be animated from the position (100,100)
    to its final x,y position within the view, over one second. The transition only applies to
    the new items that are added to the view; it does not apply to the items below that are
    displaced by the addition of the new items. To animate the displaced items, set the \l displaced
    or \l addDisplaced properties.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \note This transition is not applied to the items that are created when the view is initially
    populated, or when the view's \l model changes. (In those cases, the \l populate transition is
    applied instead.) Additionally, this transition should \e not animate the height of the new item;
    doing so will cause any items beneath the new item to be laid out at the wrong position. Instead,
    the height can be animated within the \l {add}{onAdd} handler in the delegate.

    \sa addDisplaced, populate, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::addDisplaced

    This property holds the transition to apply to items within the view that are displaced by
    the addition of other items to the view.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        addDisplaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    Whenever an item is added to the above view, all items beneath the new item are displaced, causing
    them to move down (or sideways, if horizontally orientated) within the view. As this
    displacement occurs, the items' movement to their new x,y positions within the view will be
    animated by a NumberAnimation over one second, as specified. This transition is not applied to
    the new item that has been added to the view; to animate the added items, set the \l add
    property.

    If an item is displaced by multiple types of operations at the same time, it is not defined as to
    whether the addDisplaced, moveDisplaced or removeDisplaced transition will be applied. Additionally,
    if it is not necessary to specify different transitions depending on whether an item is displaced
    by an add, move or remove operation, consider setting the \l displaced property instead.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \note This transition is not applied to the items that are created when the view is initially
    populated, or when the view's \l model changes. In those cases, the \l populate transition is
    applied instead.

    \sa displaced, add, populate, ViewTransition
*/
/*!
    \qmlproperty Transition QtQuick::GridView::move

    This property holds the transition to apply to items in the view that are being moved due
    to a move operation in the view's \l model.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        move: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    Whenever the \l model performs a move operation to move a particular set of indexes, the
    respective items in the view will be animated to their new positions in the view over one
    second. The transition only applies to the items that are the subject of the move operation
    in the model; it does not apply to items below them that are displaced by the move operation.
    To animate the displaced items, set the \l displaced or \l moveDisplaced properties.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa moveDisplaced, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::moveDisplaced

    This property holds the transition to apply to items that are displaced by a move operation in
    the view's \l model.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        moveDisplaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    Whenever the \l model performs a move operation to move a particular set of indexes, the items
    between the source and destination indexes of the move operation are displaced, causing them
    to move upwards or downwards (or sideways, if horizontally orientated) within the view. As this
    displacement occurs, the items' movement to their new x,y positions within the view will be
    animated by a NumberAnimation over one second, as specified. This transition is not applied to
    the items that are the actual subjects of the move operation; to animate the moved items, set
    the \l move property.

    If an item is displaced by multiple types of operations at the same time, it is not defined as to
    whether the addDisplaced, moveDisplaced or removeDisplaced transition will be applied. Additionally,
    if it is not necessary to specify different transitions depending on whether an item is displaced
    by an add, move or remove operation, consider setting the \l displaced property instead.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa displaced, move, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::remove

    This property holds the transition to apply to items that are removed from the view.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        remove: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: 1000 }
                NumberAnimation { properties: "x,y"; to: 100; duration: 1000 }
            }
        }
    }
    \endcode

    Whenever an item is removed from the above view, the item will be animated to the position (100,100)
    over one second, and in parallel will also change its opacity to 0. The transition
    only applies to the items that are removed from the view; it does not apply to the items below
    them that are displaced by the removal of the items. To animate the displaced items, set the
    \l displaced or \l removeDisplaced properties.

    Note that by the time the transition is applied, the item has already been removed from the
    model; any references to the model data for the removed index will not be valid.

    Additionally, if the \l delayRemove attached property has been set for a delegate item, the
    remove transition will not be applied until \l delayRemove becomes false again.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa removeDisplaced, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::removeDisplaced

    This property holds the transition to apply to items in the view that are displaced by the
    removal of other items in the view.

    For example, here is a view that specifies such a transition:

    \code
    GridView {
        ...
        removeDisplaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    Whenever an item is removed from the above view, all items beneath it are displaced, causing
    them to move upwards (or sideways, if horizontally orientated) within the view. As this
    displacement occurs, the items' movement to their new x,y positions within the view will be
    animated by a NumberAnimation over one second, as specified. This transition is not applied to
    the item that has actually been removed from the view; to animate the removed items, set the
    \l remove property.

    If an item is displaced by multiple types of operations at the same time, it is not defined as to
    whether the addDisplaced, moveDisplaced or removeDisplaced transition will be applied. Additionally,
    if it is not necessary to specify different transitions depending on whether an item is displaced
    by an add, move or remove operation, consider setting the \l displaced property instead.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa displaced, remove, ViewTransition
*/

/*!
    \qmlproperty Transition QtQuick::GridView::displaced
    This property holds the generic transition to apply to items that have been displaced by
    any model operation that affects the view.

    This is a convenience for specifying a generic transition for items that are displaced
    by add, move or remove operations, without having to specify the individual addDisplaced,
    moveDisplaced and removeDisplaced properties. For example, here is a view that specifies
    a displaced transition:

    \code
    GridView {
        ...
        displaced: Transition {
            NumberAnimation { properties: "x,y"; duration: 1000 }
        }
    }
    \endcode

    When any item is added, moved or removed within the above view, the items below it are
    displaced, causing them to move down (or sideways, if horizontally orientated) within the
    view. As this displacement occurs, the items' movement to their new x,y positions within
    the view will be animated by a NumberAnimation over one second, as specified.

    If a view specifies this generic displaced transition as well as a specific addDisplaced,
    moveDisplaced or removeDisplaced transition, the more specific transition will be used
    instead of the generic displaced transition when the relevant operation occurs, providing that
    the more specific transition has not been disabled (by setting \l {Transition::enabled}{enabled}
    to false). If it has indeed been disabled, the generic displaced transition is applied instead.

    For more details and examples on how to use view transitions, see the ViewTransition
    documentation.

    \sa addDisplaced, moveDisplaced, removeDisplaced, ViewTransition
*/

void QQuickGridView::viewportMoved(Qt::Orientations orient)
{
    Q_D(QQuickGridView);
    QQuickItemView::viewportMoved(orient);
    if (!d->itemCount)
        return;
    if (d->inViewportMoved)
        return;
    d->inViewportMoved = true;

    if (yflick()) {
        if (d->isContentFlowReversed())
            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickItemViewPrivate::BufferAfter : QQuickItemViewPrivate::BufferBefore;
        else
            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickItemViewPrivate::BufferBefore : QQuickItemViewPrivate::BufferAfter;
    } else {
        if (d->isContentFlowReversed())
            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickItemViewPrivate::BufferAfter : QQuickItemViewPrivate::BufferBefore;
        else
            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickItemViewPrivate::BufferBefore : QQuickItemViewPrivate::BufferAfter;
    }

    d->refillOrLayout();

    // Set visibility of items to eliminate cost of items outside the visible area.
    qreal from = d->isContentFlowReversed() ? -d->position()-d->displayMarginBeginning-d->size() : d->position()-d->displayMarginBeginning;
    qreal to = d->isContentFlowReversed() ? -d->position()+d->displayMarginEnd : d->position()+d->size()+d->displayMarginEnd;
    for (int i = 0; i < d->visibleItems.count(); ++i) {
        FxGridItemSG *item = static_cast<FxGridItemSG*>(d->visibleItems.at(i));
        QQuickItemPrivate::get(item->item)->setCulled(item->rowPos() + d->rowSize() < from || item->rowPos() > to);
    }
    if (d->currentItem) {
        FxGridItemSG *item = static_cast<FxGridItemSG*>(d->currentItem);
        QQuickItemPrivate::get(item->item)->setCulled(item->rowPos() + d->rowSize() < from || item->rowPos() > to);
    }

    if (d->hData.flicking || d->vData.flicking || d->hData.moving || d->vData.moving)
        d->moveReason = QQuickGridViewPrivate::Mouse;
    if (d->moveReason != QQuickGridViewPrivate::SetIndex) {
        if (d->haveHighlightRange && d->highlightRange == StrictlyEnforceRange && d->highlight) {
            // reposition highlight
            qreal pos = d->highlight->position();
            qreal viewPos = d->isContentFlowReversed() ? -d->position()-d->size() : d->position();
            if (pos > viewPos + d->highlightRangeEnd - d->highlight->size())
                pos = viewPos + d->highlightRangeEnd - d->highlight->size();
            if (pos < viewPos + d->highlightRangeStart)
                pos = viewPos + d->highlightRangeStart;

            if (pos != d->highlight->position()) {
                d->highlightXAnimator->stop();
                d->highlightYAnimator->stop();
                static_cast<FxGridItemSG*>(d->highlight)->setPosition(static_cast<FxGridItemSG*>(d->highlight)->colPos(), pos);
            } else {
                d->updateHighlight();
            }

            // update current index
            int idx = d->snapIndex();
            if (idx >= 0 && idx != d->currentIndex) {
                d->updateCurrent(idx);
                if (d->currentItem && static_cast<FxGridItemSG*>(d->currentItem)->colPos() != static_cast<FxGridItemSG*>(d->highlight)->colPos() && d->autoHighlight) {
                    if (d->flow == FlowLeftToRight)
                        d->highlightXAnimator->to = d->currentItem->itemX();
                    else
                        d->highlightYAnimator->to = d->currentItem->itemY();
                }
            }
        }
    }

    d->inViewportMoved = false;
}

void QQuickGridView::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickGridView);
    if (d->model && d->model->count() && ((d->interactive && !d->explicitKeyNavigationEnabled)
        || (d->explicitKeyNavigationEnabled && d->keyNavigationEnabled))) {
        d->moveReason = QQuickGridViewPrivate::SetIndex;
        int oldCurrent = currentIndex();
        switch (event->key()) {
        case Qt::Key_Up:
            moveCurrentIndexUp();
            break;
        case Qt::Key_Down:
            moveCurrentIndexDown();
            break;
        case Qt::Key_Left:
            moveCurrentIndexLeft();
            break;
        case Qt::Key_Right:
            moveCurrentIndexRight();
            break;
        default:
            break;
        }
        if (oldCurrent != currentIndex() || d->wrap) {
            event->accept();
            return;
        }
    }
    event->ignore();
    QQuickItemView::keyPressEvent(event);
}

void QQuickGridView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickGridView);
    d->resetColumns();

    if (newGeometry.width() != oldGeometry.width()
            && newGeometry.height() != oldGeometry.height()) {
        d->setPosition(d->position());
    } else if (newGeometry.width() != oldGeometry.width()) {
        QQuickFlickable::setContentX(d->contentXForPosition(d->position()));
    } else if (newGeometry.height() != oldGeometry.height()) {
        QQuickFlickable::setContentY(d->contentYForPosition(d->position()));
    }

    QQuickItemView::geometryChanged(newGeometry, oldGeometry);
}

void QQuickGridView::initItem(int index, QObject *obj)
{
    QQuickItemView::initItem(index, obj);

    // setting the view from the FxViewItem wrapper is too late if the delegate
    // needs access to the view in Component.onCompleted
    QQuickItem *item = qmlobject_cast<QQuickItem*>(obj);
    if (item) {
        QQuickGridViewAttached *attached = static_cast<QQuickGridViewAttached *>(
                qmlAttachedPropertiesObject<QQuickGridView>(item));
        if (attached)
            attached->setView(this);
    }
}

/*!
    \qmlmethod QtQuick::GridView::moveCurrentIndexUp()

    Move the currentIndex up one item in the view.
    The current index will wrap if keyNavigationWraps is true and it
    is currently at the end. This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/


void QQuickGridView::moveCurrentIndexUp()
{
    Q_D(QQuickGridView);
    const int count = d->model ? d->model->count() : 0;
    if (!count)
        return;
    if (d->verticalLayoutDirection == QQuickItemView::TopToBottom) {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() >= d->columns || d->wrap) {
                int index = currentIndex() - d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        } else {
            if (currentIndex() > 0 || d->wrap) {
                int index = currentIndex() - 1;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        }
    } else {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() < count - d->columns || d->wrap) {
                int index = currentIndex()+d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        } else {
            if (currentIndex() < count - 1 || d->wrap) {
                int index = currentIndex() + 1;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        }
    }
}

/*!
    \qmlmethod QtQuick::GridView::moveCurrentIndexDown()

    Move the currentIndex down one item in the view.
    The current index will wrap if keyNavigationWraps is true and it
    is currently at the end. This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickGridView::moveCurrentIndexDown()
{
    Q_D(QQuickGridView);
    const int count = d->model ? d->model->count() : 0;
    if (!count)
        return;

    if (d->verticalLayoutDirection == QQuickItemView::TopToBottom) {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() < count - d->columns || d->wrap) {
                int index = currentIndex()+d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        } else {
            if (currentIndex() < count - 1 || d->wrap) {
                int index = currentIndex() + 1;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        }
    } else {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() >= d->columns || d->wrap) {
                int index = currentIndex() - d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        } else {
            if (currentIndex() > 0 || d->wrap) {
                int index = currentIndex() - 1;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        }
    }
}

/*!
    \qmlmethod QtQuick::GridView::moveCurrentIndexLeft()

    Move the currentIndex left one item in the view.
    The current index will wrap if keyNavigationWraps is true and it
    is currently at the end. This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickGridView::moveCurrentIndexLeft()
{
    Q_D(QQuickGridView);
    const int count = d->model ? d->model->count() : 0;
    if (!count)
        return;
    if (effectiveLayoutDirection() == Qt::LeftToRight) {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() > 0 || d->wrap) {
                int index = currentIndex() - 1;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        } else {
            if (currentIndex() >= d->columns || d->wrap) {
                int index = currentIndex() - d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        }
    } else {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() < count - 1 || d->wrap) {
                int index = currentIndex() + 1;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        } else {
            if (currentIndex() < count - d->columns || d->wrap) {
                int index = currentIndex() + d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        }
    }
}


/*!
    \qmlmethod QtQuick::GridView::moveCurrentIndexRight()

    Move the currentIndex right one item in the view.
    The current index will wrap if keyNavigationWraps is true and it
    is currently at the end. This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickGridView::moveCurrentIndexRight()
{
    Q_D(QQuickGridView);
    const int count = d->model ? d->model->count() : 0;
    if (!count)
        return;
    if (effectiveLayoutDirection() == Qt::LeftToRight) {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() < count - 1 || d->wrap) {
                int index = currentIndex() + 1;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        } else {
            if (currentIndex() < count - d->columns || d->wrap) {
                int index = currentIndex()+d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : 0);
            }
        }
    } else {
        if (d->flow == QQuickGridView::FlowLeftToRight) {
            if (currentIndex() > 0 || d->wrap) {
                int index = currentIndex() - 1;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        } else {
            if (currentIndex() >= d->columns || d->wrap) {
                int index = currentIndex() - d->columns;
                setCurrentIndex((index >= 0 && index < count) ? index : count-1);
            }
        }
    }
}

bool QQuickGridViewPrivate::applyInsertionChange(const QQmlChangeSet::Change &change, ChangeResult *insertResult, QList<FxViewItem *> *addedItems, QList<MovedItem> *movingIntoView)
{
    Q_Q(QQuickGridView);

    int modelIndex = change.index;
    int count = change.count;

    int index = visibleItems.count() ? mapFromModel(modelIndex) : 0;

    if (index < 0) {
        int i = visibleItems.count() - 1;
        while (i > 0 && visibleItems.at(i)->index == -1)
            --i;
        if (visibleItems.at(i)->index + 1 == modelIndex) {
            // Special case of appending an item to the model.
            index = visibleItems.count();
        } else {
            if (modelIndex <= visibleIndex) {
                // Insert before visible items
                visibleIndex += count;
                for (int i = 0; i < visibleItems.count(); ++i) {
                    FxViewItem *item = visibleItems.at(i);
                    if (item->index != -1 && item->index >= modelIndex)
                        item->index += count;
                }
            }
            return true;
        }
    }

    qreal tempPos = isContentFlowReversed() ? -position()-size()+q->width()+1 : position();
    qreal colPos = 0;
    qreal rowPos = 0;
    int colNum = 0;
    if (visibleItems.count()) {
        if (index < visibleItems.count()) {
            FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(visibleItems.at(index));
            colPos = gridItem->colPos();
            rowPos = gridItem->rowPos();
            colNum = qFloor((colPos+colSize()/2) / colSize());
        } else {
            // appending items to visible list
            FxGridItemSG *gridItem = static_cast<FxGridItemSG*>(visibleItems.at(index-1));
            rowPos = gridItem->rowPos();
            colNum = qFloor((gridItem->colPos()+colSize()/2) / colSize());
            if (++colNum >= columns) {
                colNum = 0;
                rowPos += rowSize();
            }
            colPos = colNum * colSize();
        }
    }

    // Update the indexes of the following visible items.
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index != -1 && item->index >= modelIndex) {
            item->index += count;
            if (change.isMove())
                item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::MoveTransition, false);
            else
                item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::AddTransition, false);
        }
    }

    int prevVisibleCount = visibleItems.count();
    if (insertResult->visiblePos.isValid() && rowPos < insertResult->visiblePos) {
        // Insert items before the visible item.
        int insertionIdx = index;
        int i = count - 1;
        int from = tempPos - buffer - displayMarginBeginning;

        if (rowPos > from && insertionIdx < visibleIndex) {
                // items won't be visible, just note the size for repositioning
                insertResult->countChangeBeforeVisible += count;
                insertResult->sizeChangesBeforeVisiblePos += ((count + columns - 1) / columns) * rowSize();
        } else {
            while (i >= 0) {
                // item is before first visible e.g. in cache buffer
                FxViewItem *item = 0;
                if (change.isMove() && (item = currentChanges.removedItems.take(change.moveKey(modelIndex + i))))
                    item->index = modelIndex + i;
                if (!item)
                    item = createItem(modelIndex + i);
                if (!item)
                    return false;

                QQuickItemPrivate::get(item->item)->setCulled(false);
                visibleItems.insert(insertionIdx, item);
                if (insertionIdx == 0)
                    insertResult->changedFirstItem = true;
                if (!change.isMove()) {
                    addedItems->append(item);
                    if (transitioner)
                        item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::AddTransition, true);
                    else
                        item->moveTo(QPointF(colPos, rowPos), true);
                }
                insertResult->sizeChangesBeforeVisiblePos += rowSize();

                if (--colNum < 0 ) {
                    colNum = columns - 1;
                    rowPos -= rowSize();
                }
                colPos = colNum * colSize();
                index++;
                i--;
            }
        }

        // There may be gaps in the index sequence of visibleItems because
        // of the index shift/update done before the insertion just above.
        // Find if there is any...
        int firstOkIdx = -1;
        for (int i = 0; i <= insertionIdx && i < visibleItems.count() - 1; i++) {
            if (visibleItems.at(i)->index + 1 != visibleItems.at(i + 1)->index) {
                firstOkIdx = i + 1;
                break;
            }
        }
        // ... and remove all the items before that one
        for (int i = 0; i < firstOkIdx; i++) {
            FxViewItem *nvItem = visibleItems.takeFirst();
            addedItems->removeOne(nvItem);
            removeItem(nvItem);
        }

    } else {
        int i = 0;
        int to = buffer+displayMarginEnd+tempPos+size()-1;
        while (i < count && rowPos <= to + rowSize()*(columns - colNum)/qreal(columns+1)) {
            FxViewItem *item = 0;
            if (change.isMove() && (item = currentChanges.removedItems.take(change.moveKey(modelIndex + i))))
                item->index = modelIndex + i;
            bool newItem = !item;
            if (!item)
                item = createItem(modelIndex + i);
            if (!item)
                return false;

            QQuickItemPrivate::get(item->item)->setCulled(false);
            visibleItems.insert(index, item);
            if (index == 0)
                insertResult->changedFirstItem = true;
            if (change.isMove()) {
                // we know this is a move target, since move displaced items that are
                // shuffled into view due to a move would be added in refill()
                if (newItem && transitioner && transitioner->canTransition(QQuickItemViewTransitioner::MoveTransition, true))
                    movingIntoView->append(MovedItem(item, change.moveKey(item->index)));
            } else {
                addedItems->append(item);
                if (transitioner)
                    item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::AddTransition, true);
                else
                    item->moveTo(QPointF(colPos, rowPos), true);
            }
            insertResult->sizeChangesAfterVisiblePos += rowSize();

            if (++colNum >= columns) {
                colNum = 0;
                rowPos += rowSize();
            }
            colPos = colNum * colSize();
            ++index;
            ++i;
        }
    }

    updateVisibleIndex();

    return visibleItems.count() > prevVisibleCount;
}

void QQuickGridViewPrivate::translateAndTransitionItemsAfter(int afterModelIndex, const ChangeResult &insertionResult, const ChangeResult &removalResult)
{
    if (!transitioner)
        return;

    int markerItemIndex = -1;
    for (int i=0; i<visibleItems.count(); i++) {
        if (visibleItems.at(i)->index == afterModelIndex) {
            markerItemIndex = i;
            break;
        }
    }
    if (markerItemIndex < 0)
        return;

    const qreal viewEndPos = isContentFlowReversed() ? -position() : position() + size();
    int countItemsRemoved = -(removalResult.sizeChangesAfterVisiblePos / rowSize());

    // account for whether first item has changed if < 1 row was removed before visible
    int changeBeforeVisible = insertionResult.countChangeBeforeVisible - removalResult.countChangeBeforeVisible;
    if (changeBeforeVisible != 0)
        countItemsRemoved += (changeBeforeVisible % columns) - (columns - 1);

    countItemsRemoved -= removalResult.countChangeAfterVisibleItems;

    for (int i=markerItemIndex+1; i<visibleItems.count() && visibleItems.at(i)->position() < viewEndPos; i++) {
        FxGridItemSG *gridItem = static_cast<FxGridItemSG *>(visibleItems.at(i));
        if (!gridItem->transitionScheduledOrRunning()) {
            qreal origRowPos = gridItem->colPos();
            qreal origColPos = gridItem->rowPos();
            int indexDiff = gridItem->index - countItemsRemoved;
            gridItem->setPosition((indexDiff % columns) * colSize(), (indexDiff / columns) * rowSize());
            gridItem->transitionNextReposition(transitioner, QQuickItemViewTransitioner::RemoveTransition, false);
            gridItem->setPosition(origRowPos, origColPos);
        }
    }
}

bool QQuickGridViewPrivate::needsRefillForAddedOrRemovedIndex(int modelIndex) const
{
    // If we add or remove items before visible items, a layout may be
    // required to ensure item 0 is in the first column.
    return modelIndex < visibleIndex;
}

/*!
    \qmlmethod QtQuick::GridView::positionViewAtIndex(int index, PositionMode mode)

    Positions the view such that the \a index is at the position specified by
    \a mode:

    \list
    \li GridView.Beginning - position item at the top (or left for \c GridView.FlowTopToBottom flow) of the view.
    \li GridView.Center - position item in the center of the view.
    \li GridView.End - position item at bottom (or right for horizontal orientation) of the view.
    \li GridView.Visible - if any part of the item is visible then take no action, otherwise
    bring the item into view.
    \li GridView.Contain - ensure the entire item is visible.  If the item is larger than
    the view the item is positioned at the top (or left for \c GridView.FlowTopToBottom flow) of the view.
    \li GridView.SnapPosition - position the item at \l preferredHighlightBegin.  This mode
    is only valid if \l highlightRangeMode is StrictlyEnforceRange or snapping is enabled
    via \l snapMode.
    \endlist

    If positioning the view at the index would cause empty space to be displayed at
    the beginning or end of the view, the view will be positioned at the boundary.

    It is not recommended to use \l {Flickable::}{contentX} or \l {Flickable::}{contentY} to position the view
    at a particular index.  This is unreliable since removing items from the start
    of the view does not cause all other items to be repositioned.
    The correct way to bring an item into view is with \c positionViewAtIndex.

    \b Note: methods should only be called after the Component has completed.  To position
    the view at startup, this method should be called by Component.onCompleted.  For
    example, to position the view at the end:

    \code
    Component.onCompleted: positionViewAtIndex(count - 1, GridView.Beginning)
    \endcode
*/

/*!
    \qmlmethod QtQuick::GridView::positionViewAtBeginning()
    \qmlmethod QtQuick::GridView::positionViewAtEnd()

    Positions the view at the beginning or end, taking into account any header or footer.

    It is not recommended to use \l {Flickable::}{contentX} or \l {Flickable::}{contentY} to position the view
    at a particular index.  This is unreliable since removing items from the start
    of the list does not cause all other items to be repositioned, and because
    the actual start of the view can vary based on the size of the delegates.

    \b Note: methods should only be called after the Component has completed.  To position
    the view at startup, this method should be called by Component.onCompleted.  For
    example, to position the view at the end on startup:

    \code
    Component.onCompleted: positionViewAtEnd()
    \endcode
*/

/*!
    \qmlmethod int QtQuick::GridView::indexAt(real x, real y)

    Returns the index of the visible item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, or the item is
    not visible -1 is returned.

    If the item is outside the visible area, -1 is returned, regardless of
    whether an item will exist at that point when scrolled into view.

    \b Note: methods should only be called after the Component has completed.
*/

/*!
    \qmlmethod Item QtQuick::GridView::itemAt(real x, real y)

    Returns the visible item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, or the item is
    not visible null is returned.

    If the item is outside the visible area, null is returned, regardless of
    whether an item will exist at that point when scrolled into view.

    \b Note: methods should only be called after the Component has completed.
*/


/*!
    \qmlmethod QtQuick::GridView::forceLayout()

    Responding to changes in the model is usually batched to happen only once
    per frame. This means that inside script blocks it is possible for the
    underlying model to have changed, but the GridView has not caught up yet.

    This method forces the GridView to immediately respond to any outstanding
    changes in the model.

    \since 5.1

    \b Note: methods should only be called after the Component has completed.
*/

QQuickGridViewAttached *QQuickGridView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickGridViewAttached(obj);
}

QT_END_NAMESPACE
