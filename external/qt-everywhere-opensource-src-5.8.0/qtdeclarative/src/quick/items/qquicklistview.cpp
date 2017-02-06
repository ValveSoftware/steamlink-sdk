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

#include "qquicklistview_p.h"
#include "qquickitemview_p_p.h"

#include <private/qqmlobjectmodel_p.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlinfo.h>
#include <QtGui/qevent.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qmath.h>

#include <private/qquicksmoothedanimation_p_p.h>
#include "qplatformdefs.h"

QT_BEGIN_NAMESPACE

#ifndef QML_FLICK_SNAPONETHRESHOLD
#define QML_FLICK_SNAPONETHRESHOLD 30
#endif

class FxListItemSG;

class QQuickListViewPrivate : public QQuickItemViewPrivate
{
    Q_DECLARE_PUBLIC(QQuickListView)
public:
    static QQuickListViewPrivate* get(QQuickListView *item) { return item->d_func(); }

    Qt::Orientation layoutOrientation() const Q_DECL_OVERRIDE;
    bool isContentFlowReversed() const Q_DECL_OVERRIDE;
    bool isRightToLeft() const;
    bool isBottomToTop() const;

    qreal positionAt(int index) const Q_DECL_OVERRIDE;
    qreal endPositionAt(int index) const Q_DECL_OVERRIDE;
    qreal originPosition() const Q_DECL_OVERRIDE;
    qreal lastPosition() const Q_DECL_OVERRIDE;

    FxViewItem *itemBefore(int modelIndex) const;
    QString sectionAt(int modelIndex);
    qreal snapPosAt(qreal pos);
    FxViewItem *snapItemAt(qreal pos);

    void init() Q_DECL_OVERRIDE;
    void clear() Q_DECL_OVERRIDE;

    bool addVisibleItems(qreal fillFrom, qreal fillTo, qreal bufferFrom, qreal bufferTo, bool doBuffer) Q_DECL_OVERRIDE;
    bool removeNonVisibleItems(qreal bufferFrom, qreal bufferTo) Q_DECL_OVERRIDE;
    void visibleItemsChanged() Q_DECL_OVERRIDE;

    void removeItem(FxViewItem *item);

    FxViewItem *newViewItem(int index, QQuickItem *item) Q_DECL_OVERRIDE;
    void initializeViewItem(FxViewItem *item) Q_DECL_OVERRIDE;
    bool releaseItem(FxViewItem *item) Q_DECL_OVERRIDE;
    void repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer) Q_DECL_OVERRIDE;
    void repositionPackageItemAt(QQuickItem *item, int index) Q_DECL_OVERRIDE;
    void resetFirstItemPosition(qreal pos = 0.0) Q_DECL_OVERRIDE;
    void adjustFirstItem(qreal forwards, qreal backwards, int) Q_DECL_OVERRIDE;
    void updateSizeChangesBeforeVisiblePos(FxViewItem *item, ChangeResult *removeResult) Q_DECL_OVERRIDE;

    void createHighlight() Q_DECL_OVERRIDE;
    void updateHighlight() Q_DECL_OVERRIDE;
    void resetHighlightPosition() Q_DECL_OVERRIDE;

    void setPosition(qreal pos) Q_DECL_OVERRIDE;
    void layoutVisibleItems(int fromModelIndex = 0) Q_DECL_OVERRIDE;

    bool applyInsertionChange(const QQmlChangeSet::Change &insert, ChangeResult *changeResult, QList<FxViewItem *> *addedItems, QList<MovedItem> *movingIntoView) Q_DECL_OVERRIDE;
    void translateAndTransitionItemsAfter(int afterIndex, const ChangeResult &insertionResult, const ChangeResult &removalResult) Q_DECL_OVERRIDE;

    void updateSectionCriteria() Q_DECL_OVERRIDE;
    void updateSections() Q_DECL_OVERRIDE;
    QQuickItem *getSectionItem(const QString &section);
    void releaseSectionItem(QQuickItem *item);
    void releaseSectionItems();
    void updateInlineSection(FxListItemSG *);
    void updateCurrentSection();
    void updateStickySections();

    qreal headerSize() const Q_DECL_OVERRIDE;
    qreal footerSize() const Q_DECL_OVERRIDE;
    bool showHeaderForIndex(int index) const Q_DECL_OVERRIDE;
    bool showFooterForIndex(int index) const Q_DECL_OVERRIDE;
    void updateHeader() Q_DECL_OVERRIDE;
    void updateFooter() Q_DECL_OVERRIDE;
    bool hasStickyHeader() const Q_DECL_OVERRIDE;
    bool hasStickyFooter() const Q_DECL_OVERRIDE;

    void changedVisibleIndex(int newIndex) Q_DECL_OVERRIDE;
    void initializeCurrentItem() Q_DECL_OVERRIDE;

    void updateAverage();

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &oldGeometry) Q_DECL_OVERRIDE;
    void fixupPosition() Q_DECL_OVERRIDE;
    void fixup(AxisData &data, qreal minExtent, qreal maxExtent) Q_DECL_OVERRIDE;
    bool flick(QQuickItemViewPrivate::AxisData &data, qreal minExtent, qreal maxExtent, qreal vSize,
               QQuickTimeLineCallback::Callback fixupCallback, qreal velocity) Q_DECL_OVERRIDE;

    QQuickListView::Orientation orient;
    qreal visiblePos;
    qreal averageSize;
    qreal spacing;
    QQuickListView::SnapMode snapMode;

    QQuickListView::HeaderPositioning headerPositioning;
    QQuickListView::FooterPositioning footerPositioning;

    QSmoothedAnimation *highlightPosAnimator;
    QSmoothedAnimation *highlightWidthAnimator;
    QSmoothedAnimation *highlightHeightAnimator;
    qreal highlightMoveVelocity;
    qreal highlightResizeVelocity;
    int highlightResizeDuration;

    QQuickViewSection *sectionCriteria;
    QString currentSection;
    static const int sectionCacheSize = 5;
    QQuickItem *sectionCache[sectionCacheSize];
    QQuickItem *currentSectionItem;
    QString currentStickySection;
    QQuickItem *nextSectionItem;
    QString nextStickySection;
    QString lastVisibleSection;
    QString nextSection;

    qreal overshootDist;
    bool correctFlick : 1;
    bool inFlickCorrection : 1;

    QQuickListViewPrivate()
        : orient(QQuickListView::Vertical)
        , visiblePos(0)
        , averageSize(100.0), spacing(0.0)
        , snapMode(QQuickListView::NoSnap)
        , headerPositioning(QQuickListView::InlineHeader)
        , footerPositioning(QQuickListView::InlineFooter)
        , highlightPosAnimator(0), highlightWidthAnimator(0), highlightHeightAnimator(0)
        , highlightMoveVelocity(400), highlightResizeVelocity(400), highlightResizeDuration(-1)
        , sectionCriteria(0), currentSectionItem(0), nextSectionItem(0)
        , overshootDist(0.0), correctFlick(false), inFlickCorrection(false)
    {
        highlightMoveDuration = -1; //override default value set in base class
    }
    ~QQuickListViewPrivate() {
        delete highlightPosAnimator;
        delete highlightWidthAnimator;
        delete highlightHeightAnimator;
    }

    friend class QQuickViewSection;
};

//----------------------------------------------------------------------------

QQuickViewSection::QQuickViewSection(QQuickListView *parent)
    : QObject(parent), m_criteria(FullString), m_delegate(0), m_labelPositioning(InlineLabels)
    , m_view(parent ? QQuickListViewPrivate::get(parent) : 0)
{
}

void QQuickViewSection::setProperty(const QString &property)
{
    if (property != m_property) {
        m_property = property;
        emit propertyChanged();
        // notify view that the contents of the sections must be recalculated
        m_view->updateSectionCriteria();
    }
}

void QQuickViewSection::setCriteria(QQuickViewSection::SectionCriteria criteria)
{
    if (criteria != m_criteria) {
        m_criteria = criteria;
        emit criteriaChanged();
        // notify view that the contents of the sections must be recalculated
        m_view->updateSectionCriteria();
    }
}

void QQuickViewSection::setDelegate(QQmlComponent *delegate)
{
    if (delegate != m_delegate) {
        if (m_delegate)
            m_view->releaseSectionItems();
        m_delegate = delegate;
        emit delegateChanged();
        m_view->forceLayoutPolish();
    }
}

QString QQuickViewSection::sectionString(const QString &value)
{
    if (m_criteria == FirstCharacter)
        return value.isEmpty() ? QString() : value.at(0);
    else
        return value;
}

void QQuickViewSection::setLabelPositioning(int l)
{
    if (m_labelPositioning != l) {
        m_labelPositioning = l;
        emit labelPositioningChanged();
        m_view->forceLayoutPolish();
    }
}

//----------------------------------------------------------------------------

class FxListItemSG : public FxViewItem
{
public:
    FxListItemSG(QQuickItem *i, QQuickListView *v, bool own) : FxViewItem(i, v, own, static_cast<QQuickItemViewAttached*>(qmlAttachedPropertiesObject<QQuickListView>(i))), view(v)
    {
    }

    inline QQuickItem *section() const {
        return item && attached ? static_cast<QQuickListViewAttached*>(attached)->m_sectionItem : 0;
    }
    void setSection(QQuickItem *s) {
        static_cast<QQuickListViewAttached*>(attached)->m_sectionItem = s;
    }

    qreal position() const Q_DECL_OVERRIDE {
        if (section()) {
            if (view->orientation() == QQuickListView::Vertical)
                return (view->verticalLayoutDirection() == QQuickItemView::BottomToTop ? -section()->height()-section()->y() : section()->y());
            else
                return (view->effectiveLayoutDirection() == Qt::RightToLeft ? -section()->width()-section()->x() : section()->x());
        } else {
            return itemPosition();
        }
    }
    qreal itemPosition() const {
        if (view->orientation() == QQuickListView::Vertical)
            return (view->verticalLayoutDirection() == QQuickItemView::BottomToTop ? -itemHeight()-itemY() : itemY());
        else
            return (view->effectiveLayoutDirection() == Qt::RightToLeft ? -itemWidth()-itemX() : itemX());
    }
    qreal size() const Q_DECL_OVERRIDE {
        if (section())
            return (view->orientation() == QQuickListView::Vertical ? itemHeight()+section()->height() : itemWidth()+section()->width());
        else
            return (view->orientation() == QQuickListView::Vertical ? itemHeight() : itemWidth());
    }
    qreal itemSize() const {
        return (view->orientation() == QQuickListView::Vertical ? itemHeight() : itemWidth());
    }
    qreal sectionSize() const Q_DECL_OVERRIDE {
        if (section())
            return (view->orientation() == QQuickListView::Vertical ? section()->height() : section()->width());
        return 0.0;
    }
    qreal endPosition() const Q_DECL_OVERRIDE {
        if (view->orientation() == QQuickListView::Vertical) {
            return (view->verticalLayoutDirection() == QQuickItemView::BottomToTop
                    ? -itemY()
                    : itemY() + itemHeight());
        } else {
            return (view->effectiveLayoutDirection() == Qt::RightToLeft
                    ? -itemX()
                    : itemX() + itemWidth());
        }
    }
    void setPosition(qreal pos, bool immediate = false) {
        // position the section immediately even if there is a transition
        if (section()) {
            if (view->orientation() == QQuickListView::Vertical) {
                if (view->verticalLayoutDirection() == QQuickItemView::BottomToTop)
                    section()->setY(-section()->height()-pos);
                else
                    section()->setY(pos);
            } else {
                if (view->effectiveLayoutDirection() == Qt::RightToLeft)
                    section()->setX(-section()->width()-pos);
                else
                    section()->setX(pos);
            }
        }
        moveTo(pointForPosition(pos), immediate);
    }
    void setSize(qreal size) {
        if (view->orientation() == QQuickListView::Vertical)
            item->setHeight(size);
        else
            item->setWidth(size);
    }
    bool contains(qreal x, qreal y) const Q_DECL_OVERRIDE {
        return (x >= itemX() && x < itemX() + itemWidth() &&
                y >= itemY() && y < itemY() + itemHeight());
    }

    QQuickListView *view;

private:
    QPointF pointForPosition(qreal pos) const {
        if (view->orientation() == QQuickListView::Vertical) {
            if (view->verticalLayoutDirection() == QQuickItemView::BottomToTop) {
                if (section())
                    pos += section()->height();
                return QPointF(itemX(), -itemHeight() - pos);
            } else {
                if (section())
                    pos += section()->height();
                return QPointF(itemX(), pos);
            }
        } else {
            if (view->effectiveLayoutDirection() == Qt::RightToLeft) {
                if (section())
                    pos += section()->width();
                return QPointF(-itemWidth() - pos, itemY());
            } else {
                if (section())
                    pos += section()->width();
                return QPointF(pos, itemY());
            }
        }
    }
};

//----------------------------------------------------------------------------

bool QQuickListViewPrivate::isContentFlowReversed() const
{
    return isRightToLeft() || isBottomToTop();
}

Qt::Orientation QQuickListViewPrivate::layoutOrientation() const
{
    return static_cast<Qt::Orientation>(orient);
}

bool QQuickListViewPrivate::isRightToLeft() const
{
    Q_Q(const QQuickListView);
    return orient == QQuickListView::Horizontal && q->effectiveLayoutDirection() == Qt::RightToLeft;
}

bool QQuickListViewPrivate::isBottomToTop() const
{
    return orient == QQuickListView::Vertical && verticalLayoutDirection == QQuickItemView::BottomToTop;
}

// Returns the item before modelIndex, if created.
// May return an item marked for removal.
FxViewItem *QQuickListViewPrivate::itemBefore(int modelIndex) const
{
    if (modelIndex < visibleIndex)
        return 0;
    int idx = 1;
    int lastIndex = -1;
    while (idx < visibleItems.count()) {
        FxViewItem *item = visibleItems.at(idx);
        if (item->index != -1)
            lastIndex = item->index;
        if (item->index == modelIndex)
            return visibleItems.at(idx-1);
        ++idx;
    }
    if (lastIndex == modelIndex-1)
        return visibleItems.constLast();
    return 0;
}

void QQuickListViewPrivate::setPosition(qreal pos)
{
    Q_Q(QQuickListView);
    if (orient == QQuickListView::Vertical) {
        if (isBottomToTop())
            q->QQuickFlickable::setContentY(-pos-size());
        else
            q->QQuickFlickable::setContentY(pos);
    } else {
        if (isRightToLeft())
            q->QQuickFlickable::setContentX(-pos-size());
        else
            q->QQuickFlickable::setContentX(pos);
    }
}

qreal QQuickListViewPrivate::originPosition() const
{
    qreal pos = 0;
    if (!visibleItems.isEmpty()) {
        pos = (*visibleItems.constBegin())->position();
        if (visibleIndex > 0)
            pos -= visibleIndex * (averageSize + spacing);
    }
    return pos;
}

qreal QQuickListViewPrivate::lastPosition() const
{
    qreal pos = 0;
    if (!visibleItems.isEmpty()) {
        int invisibleCount = INT_MIN;
        int delayRemovedCount = 0;
        for (int i = visibleItems.count()-1; i >= 0; --i) {
            if (visibleItems.at(i)->index != -1) {
                // Find the invisible count after the last visible item with known index
                invisibleCount = model->count() - (visibleItems.at(i)->index + 1 + delayRemovedCount);
                break;
            } else if (visibleItems.at(i)->attached->delayRemove()) {
                ++delayRemovedCount;
            }
        }
        if (invisibleCount == INT_MIN) {
            // All visible items are in delayRemove state
            invisibleCount = model->count();
        }
        pos = (*(--visibleItems.constEnd()))->endPosition();
        if (invisibleCount > 0)
            pos += invisibleCount * (averageSize + spacing);
    } else if (model && model->count()) {
        pos = (model->count() * averageSize + (model->count()-1) * spacing);
    }
    return pos;
}

qreal QQuickListViewPrivate::positionAt(int modelIndex) const
{
    if (FxViewItem *item = visibleItem(modelIndex)) {
        return item->position();
    }
    if (!visibleItems.isEmpty()) {
        if (modelIndex < visibleIndex) {
            int count = visibleIndex - modelIndex;
            qreal cs = 0;
            if (modelIndex == currentIndex && currentItem) {
                cs = currentItem->size() + spacing;
                --count;
            }
            return (*visibleItems.constBegin())->position() - count * (averageSize + spacing) - cs;
        } else {
            int count = modelIndex - findLastVisibleIndex(visibleIndex) - 1;
            return (*(--visibleItems.constEnd()))->endPosition() + spacing + count * (averageSize + spacing);
        }
    }
    return 0;
}

qreal QQuickListViewPrivate::endPositionAt(int modelIndex) const
{
    if (FxViewItem *item = visibleItem(modelIndex))
        return item->endPosition();
    if (!visibleItems.isEmpty()) {
        if (modelIndex < visibleIndex) {
            int count = visibleIndex - modelIndex;
            return (*visibleItems.constBegin())->position() - (count - 1) * (averageSize + spacing) - spacing;
        } else {
            int count = modelIndex - findLastVisibleIndex(visibleIndex) - 1;
            return (*(--visibleItems.constEnd()))->endPosition() + count * (averageSize + spacing);
        }
    }
    return 0;
}

QString QQuickListViewPrivate::sectionAt(int modelIndex)
{
    if (FxViewItem *item = visibleItem(modelIndex))
        return item->attached->section();

    QString section;
    if (sectionCriteria && modelIndex >= 0 && modelIndex < itemCount) {
        QString propValue = model->stringValue(modelIndex, sectionCriteria->property());
        section = sectionCriteria->sectionString(propValue);
    }

    return section;
}

qreal QQuickListViewPrivate::snapPosAt(qreal pos)
{
    if (FxViewItem *snapItem = snapItemAt(pos))
        return snapItem->position();
    if (visibleItems.count()) {
        qreal firstPos = (*visibleItems.constBegin())->position();
        qreal endPos = (*(--visibleItems.constEnd()))->position();
        if (pos < firstPos) {
            return firstPos - qRound((firstPos - pos) / averageSize) * averageSize;
        } else if (pos > endPos)
            return endPos + qRound((pos - endPos) / averageSize) * averageSize;
    }
    return qRound((pos - originPosition()) / averageSize) * averageSize + originPosition();
}

FxViewItem *QQuickListViewPrivate::snapItemAt(qreal pos)
{
    FxViewItem *snapItem = 0;
    qreal prevItemSize = 0;
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxViewItem *item = visibleItems.at(i);
        if (item->index == -1)
            continue;
        qreal itemTop = item->position();
        if (highlight && itemTop >= pos && item->endPosition() <= pos + highlight->size())
            return item;
        if (itemTop+item->size()/2 >= pos && itemTop-prevItemSize/2 < pos)
            snapItem = item;
        prevItemSize = item->size();
    }
    return snapItem;
}

void QQuickListViewPrivate::changedVisibleIndex(int newIndex)
{
    visiblePos = positionAt(newIndex);
    visibleIndex = newIndex;
}

void QQuickListViewPrivate::init()
{
    QQuickItemViewPrivate::init();
    ::memset(sectionCache, 0, sizeof(QQuickItem*) * sectionCacheSize);
}

void QQuickListViewPrivate::clear()
{
    for (int i = 0; i < sectionCacheSize; ++i) {
        delete sectionCache[i];
        sectionCache[i] = 0;
    }
    visiblePos = 0;
    releaseSectionItem(currentSectionItem);
    currentSectionItem = 0;
    releaseSectionItem(nextSectionItem);
    nextSectionItem = 0;
    lastVisibleSection = QString();
    QQuickItemViewPrivate::clear();
}

FxViewItem *QQuickListViewPrivate::newViewItem(int modelIndex, QQuickItem *item)
{
    Q_Q(QQuickListView);

    FxListItemSG *listItem = new FxListItemSG(item, q, false);
    listItem->index = modelIndex;

    // initialise attached properties
    if (sectionCriteria) {
        QString propValue = model->stringValue(modelIndex, sectionCriteria->property());
        QString section = sectionCriteria->sectionString(propValue);
        QString prevSection;
        QString nextSection;
        if (modelIndex > 0) {
            if (FxViewItem *item = itemBefore(modelIndex))
                prevSection = item->attached->section();
            else
                prevSection = sectionAt(modelIndex-1);
        }
        if (modelIndex < model->count()-1) {
            nextSection = sectionAt(modelIndex+1);
        }
        listItem->attached->setSections(prevSection, section, nextSection);
    }

    return listItem;
}

void QQuickListViewPrivate::initializeViewItem(FxViewItem *item)
{
    QQuickItemViewPrivate::initializeViewItem(item);

    // need to track current items that are animating
    item->trackGeometry(true);

    if (sectionCriteria && sectionCriteria->delegate()) {
        if (QString::compare(item->attached->m_prevSection, item->attached->m_section, Qt::CaseInsensitive))
            updateInlineSection(static_cast<FxListItemSG*>(item));
    }
}

bool QQuickListViewPrivate::releaseItem(FxViewItem *item)
{
    if (!item || !model)
        return true;

    QPointer<QQuickItem> it = item->item;
    QQuickListViewAttached *att = static_cast<QQuickListViewAttached*>(item->attached);

    bool released = QQuickItemViewPrivate::releaseItem(item);
    if (released && it && att && att->m_sectionItem) {
        // We hold no more references to this item
        int i = 0;
        do {
            if (!sectionCache[i]) {
                sectionCache[i] = att->m_sectionItem;
                sectionCache[i]->setVisible(false);
                att->m_sectionItem = 0;
                break;
            }
            ++i;
        } while (i < sectionCacheSize);
        delete att->m_sectionItem;
        att->m_sectionItem = 0;
    }

    return released;
}

bool QQuickListViewPrivate::addVisibleItems(qreal fillFrom, qreal fillTo, qreal bufferFrom, qreal bufferTo, bool doBuffer)
{
    qreal itemEnd = visiblePos;
    if (visibleItems.count()) {
        visiblePos = (*visibleItems.constBegin())->position();
        itemEnd = (*(--visibleItems.constEnd()))->endPosition() + spacing;
    }

    int modelIndex = findLastVisibleIndex();
    bool haveValidItems = modelIndex >= 0;
    modelIndex = modelIndex < 0 ? visibleIndex : modelIndex + 1;

    if (haveValidItems && (bufferFrom > itemEnd+averageSize+spacing
        || bufferTo < visiblePos - averageSize - spacing)) {
        // We've jumped more than a page.  Estimate which items are now
        // visible and fill from there.
        int count = (fillFrom - itemEnd) / (averageSize + spacing);
        int newModelIdx = qBound(0, modelIndex + count, model->count());
        count = newModelIdx - modelIndex;
        if (count) {
            for (int i = 0; i < visibleItems.count(); ++i)
                releaseItem(visibleItems.at(i));
            visibleItems.clear();
            modelIndex = newModelIdx;
            visibleIndex = modelIndex;
            visiblePos = itemEnd + count * (averageSize + spacing);
            itemEnd = visiblePos;
        }
    }

    bool changed = false;
    FxListItemSG *item = 0;
    qreal pos = itemEnd;
    while (modelIndex < model->count() && pos <= fillTo) {
        if (!(item = static_cast<FxListItemSG*>(createItem(modelIndex, doBuffer))))
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: append item" << modelIndex << "pos" << pos << "buffer" << doBuffer << "item" << (QObject *)(item->item);
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(pos, true);
        if (item->item)
            QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        pos += item->size() + spacing;
        visibleItems.append(item);
        ++modelIndex;
        changed = true;
    }

    if (doBuffer && requestedIndex != -1) // already waiting for an item
        return changed;

    while (visibleIndex > 0 && visibleIndex <= model->count() && visiblePos > fillFrom) {
        if (!(item = static_cast<FxListItemSG*>(createItem(visibleIndex-1, doBuffer))))
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: prepend item" << visibleIndex-1 << "current top pos" << visiblePos << "buffer" << doBuffer << "item" << (QObject *)(item->item);
        --visibleIndex;
        visiblePos -= item->size() + spacing;
        if (!transitioner || !transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true)) // pos will be set by layoutVisibleItems()
            item->setPosition(visiblePos, true);
        if (item->item)
            QQuickItemPrivate::get(item->item)->setCulled(doBuffer);
        visibleItems.prepend(item);
        changed = true;
    }

    return changed;
}

void QQuickListViewPrivate::removeItem(FxViewItem *item)
{
    if (item->transitionScheduledOrRunning()) {
        qCDebug(lcItemViewDelegateLifecycle) << "\tnot releasing animating item" << item->index << (QObject *)(item->item);
        item->releaseAfterTransition = true;
        releasePendingTransition.append(item);
    } else {
        qCDebug(lcItemViewDelegateLifecycle) << "\treleasing stationary item" << item->index << (QObject *)(item->item);
        releaseItem(item);
    }
}

bool QQuickListViewPrivate::removeNonVisibleItems(qreal bufferFrom, qreal bufferTo)
{
    FxViewItem *item = 0;
    bool changed = false;

    // Remove items from the start of the view.
    // Zero-sized items shouldn't be removed unless a non-zero-sized item is also being
    // removed, otherwise a zero-sized item is infinitely added and removed over and
    // over by refill().
    int index = 0;
    while (visibleItems.count() > 1 && index < visibleItems.count()
           && (item = visibleItems.at(index)) && item->endPosition() < bufferFrom) {
        if (item->attached->delayRemove())
            break;

        if (item->size() > 0) {
            qCDebug(lcItemViewDelegateLifecycle) << "refill: remove first" << visibleIndex << "top end pos" << item->endPosition();
            // remove this item and all zero-sized items before it
            while (item) {
                if (item->index != -1)
                    visibleIndex++;
                visibleItems.removeAt(index);
                removeItem(item);
                if (index == 0)
                    break;
                item = visibleItems.at(--index);
            }
            changed = true;
        } else {
            index++;
        }
    }

    while (visibleItems.count() > 1 && (item = visibleItems.constLast()) && item->position() > bufferTo) {
        if (item->attached->delayRemove())
            break;
        qCDebug(lcItemViewDelegateLifecycle) << "refill: remove last" << visibleIndex+visibleItems.count()-1 << item->position() << (QObject *)(item->item);
        visibleItems.removeLast();
        removeItem(item);
        changed = true;
    }

    return changed;
}

void QQuickListViewPrivate::visibleItemsChanged()
{
    if (visibleItems.count())
        visiblePos = (*visibleItems.constBegin())->position();
    updateAverage();
    if (currentIndex >= 0 && currentItem && !visibleItem(currentIndex)) {
        static_cast<FxListItemSG*>(currentItem)->setPosition(positionAt(currentIndex));
        updateHighlight();
    }
    if (sectionCriteria)
        updateCurrentSection();
    updateUnrequestedPositions();
}

void QQuickListViewPrivate::layoutVisibleItems(int fromModelIndex)
{
    if (!visibleItems.isEmpty()) {
        const qreal from = isContentFlowReversed() ? -position()-displayMarginBeginning-size() : position()-displayMarginBeginning;
        const qreal to = isContentFlowReversed() ? -position()+displayMarginEnd : position()+size()+displayMarginEnd;

        FxViewItem *firstItem = *visibleItems.constBegin();
        bool fixedCurrent = currentItem && firstItem->item == currentItem->item;
        qreal sum = firstItem->size();
        qreal pos = firstItem->position() + firstItem->size() + spacing;
        firstItem->setVisible(firstItem->endPosition() >= from && firstItem->position() <= to);

        for (int i=1; i < visibleItems.count(); ++i) {
            FxListItemSG *item = static_cast<FxListItemSG*>(visibleItems.at(i));
            if (item->index >= fromModelIndex) {
                item->setPosition(pos);
                item->setVisible(item->endPosition() >= from && item->position() <= to);
            }
            pos += item->size() + spacing;
            sum += item->size();
            fixedCurrent = fixedCurrent || (currentItem && item->item == currentItem->item);
        }
        averageSize = qRound(sum / visibleItems.count());

        // move current item if it is not a visible item.
        if (currentIndex >= 0 && currentItem && !fixedCurrent)
            static_cast<FxListItemSG*>(currentItem)->setPosition(positionAt(currentIndex));

        updateCurrentSection();
        updateStickySections();
    }
}

void QQuickListViewPrivate::repositionItemAt(FxViewItem *item, int index, qreal sizeBuffer)
{
    static_cast<FxListItemSG *>(item)->setPosition(positionAt(index) + sizeBuffer);
}

void QQuickListViewPrivate::repositionPackageItemAt(QQuickItem *item, int index)
{
    Q_Q(QQuickListView);
    qreal pos = position();
    if (orient == QQuickListView::Vertical) {
        if (item->y() + item->height() > pos && item->y() < pos + q->height()) {
            if (isBottomToTop())
                item->setY(-positionAt(index)-item->height());
            else
                item->setY(positionAt(index));
        }
    } else {
        if (item->x() + item->width() > pos && item->x() < pos + q->width()) {
            if (isRightToLeft())
                item->setX(-positionAt(index)-item->width());
            else
                item->setX(positionAt(index));
        }
    }
}

void QQuickListViewPrivate::resetFirstItemPosition(qreal pos)
{
    FxListItemSG *item = static_cast<FxListItemSG*>(visibleItems.constFirst());
    item->setPosition(pos);
}

void QQuickListViewPrivate::adjustFirstItem(qreal forwards, qreal backwards, int)
{
    if (!visibleItems.count())
        return;
    qreal diff = forwards - backwards;
    static_cast<FxListItemSG*>(visibleItems.constFirst())->setPosition(visibleItems.constFirst()->position() + diff);
}

void QQuickListViewPrivate::updateSizeChangesBeforeVisiblePos(FxViewItem *item, ChangeResult *removeResult)
{
    if (item != visibleItems.constFirst())
        QQuickItemViewPrivate::updateSizeChangesBeforeVisiblePos(item, removeResult);
}

void QQuickListViewPrivate::createHighlight()
{
    Q_Q(QQuickListView);
    bool changed = false;
    if (highlight) {
        if (trackedItem == highlight)
            trackedItem = 0;
        delete highlight;
        highlight = 0;

        delete highlightPosAnimator;
        delete highlightWidthAnimator;
        delete highlightHeightAnimator;
        highlightPosAnimator = 0;
        highlightWidthAnimator = 0;
        highlightHeightAnimator = 0;

        changed = true;
    }

    if (currentItem) {
        QQuickItem *item = createHighlightItem();
        if (item) {
            FxListItemSG *newHighlight = new FxListItemSG(item, q, true);
            newHighlight->trackGeometry(true);

            if (autoHighlight) {
                newHighlight->setSize(static_cast<FxListItemSG*>(currentItem)->itemSize());
                newHighlight->setPosition(static_cast<FxListItemSG*>(currentItem)->itemPosition());
            }
            const QLatin1String posProp(orient == QQuickListView::Vertical ? "y" : "x");
            highlightPosAnimator = new QSmoothedAnimation;
            highlightPosAnimator->target = QQmlProperty(item, posProp);
            highlightPosAnimator->velocity = highlightMoveVelocity;
            highlightPosAnimator->userDuration = highlightMoveDuration;

            highlightWidthAnimator = new QSmoothedAnimation;
            highlightWidthAnimator->velocity = highlightResizeVelocity;
            highlightWidthAnimator->userDuration = highlightResizeDuration;
            highlightWidthAnimator->target = QQmlProperty(item, QStringLiteral("width"));

            highlightHeightAnimator = new QSmoothedAnimation;
            highlightHeightAnimator->velocity = highlightResizeVelocity;
            highlightHeightAnimator->userDuration = highlightResizeDuration;
            highlightHeightAnimator->target = QQmlProperty(item, QStringLiteral("height"));

            highlight = newHighlight;
            changed = true;
        }
    }
    if (changed)
        emit q->highlightItemChanged();
}

void QQuickListViewPrivate::updateHighlight()
{
    applyPendingChanges();

    if ((!currentItem && highlight) || (currentItem && !highlight))
        createHighlight();
    bool strictHighlight = haveHighlightRange && highlightRange == QQuickListView::StrictlyEnforceRange;
    if (currentItem && autoHighlight && highlight && (!strictHighlight || !pressed)) {
        // auto-update highlight
        FxListItemSG *listItem = static_cast<FxListItemSG*>(currentItem);
        highlightPosAnimator->to = isContentFlowReversed()
                ? -listItem->itemPosition()-listItem->itemSize()
                : listItem->itemPosition();
        highlightWidthAnimator->to = listItem->item->width();
        highlightHeightAnimator->to = listItem->item->height();
        if (orient == QQuickListView::Vertical) {
            if (highlight->item->width() == 0)
                highlight->item->setWidth(currentItem->item->width());
        } else {
            if (highlight->item->height() == 0)
                highlight->item->setHeight(currentItem->item->height());
        }

        highlightPosAnimator->restart();
        highlightWidthAnimator->restart();
        highlightHeightAnimator->restart();
    }
    updateTrackedItem();
}

void QQuickListViewPrivate::resetHighlightPosition()
{
    if (highlight && currentItem)
        static_cast<FxListItemSG*>(highlight)->setPosition(static_cast<FxListItemSG*>(currentItem)->itemPosition());
}

QQuickItem * QQuickListViewPrivate::getSectionItem(const QString &section)
{
    Q_Q(QQuickListView);
    QQuickItem *sectionItem = 0;
    int i = sectionCacheSize-1;
    while (i >= 0 && !sectionCache[i])
        --i;
    if (i >= 0) {
        sectionItem = sectionCache[i];
        sectionCache[i] = 0;
        sectionItem->setVisible(true);
        QQmlContext *context = QQmlEngine::contextForObject(sectionItem)->parentContext();
        context->setContextProperty(QLatin1String("section"), section);
    } else {
        QQmlContext *creationContext = sectionCriteria->delegate()->creationContext();
        QQmlContext *context = new QQmlContext(
                creationContext ? creationContext : qmlContext(q));
        context->setContextProperty(QLatin1String("section"), section);
        QObject *nobj = sectionCriteria->delegate()->beginCreate(context);
        if (nobj) {
            QQml_setParent_noEvent(context, nobj);
            sectionItem = qobject_cast<QQuickItem *>(nobj);
            if (!sectionItem) {
                delete nobj;
            } else {
                if (qFuzzyIsNull(sectionItem->z()))
                    sectionItem->setZ(2);
                QQml_setParent_noEvent(sectionItem, contentItem);
                sectionItem->setParentItem(contentItem);
            }
            // sections are not controlled by FxListItemSG, so apply attached properties here
            QQuickItemViewAttached *attached = static_cast<QQuickItemViewAttached*>(qmlAttachedPropertiesObject<QQuickListView>(sectionItem));
            attached->setView(q);
        } else {
            delete context;
        }
        sectionCriteria->delegate()->completeCreate();
    }

    return sectionItem;
}

void QQuickListViewPrivate::releaseSectionItem(QQuickItem *item)
{
    if (!item)
        return;
    int i = 0;
    do {
        if (!sectionCache[i]) {
            sectionCache[i] = item;
            sectionCache[i]->setVisible(false);
            return;
        }
        ++i;
    } while (i < sectionCacheSize);
    delete item;
}


void QQuickListViewPrivate::releaseSectionItems()
{
    for (int i = 0; i < visibleItems.count(); ++i) {
        FxListItemSG *listItem = static_cast<FxListItemSG *>(visibleItems.at(i));
        if (listItem->section()) {
            qreal pos = listItem->position();
            releaseSectionItem(listItem->section());
            listItem->setSection(0);
            listItem->setPosition(pos);
        }
    }
    for (int i = 0; i < sectionCacheSize; ++i) {
        delete sectionCache[i];
        sectionCache[i] = 0;
    }
}

void QQuickListViewPrivate::updateInlineSection(FxListItemSG *listItem)
{
    if (!sectionCriteria || !sectionCriteria->delegate())
        return;
    if (QString::compare(listItem->attached->m_prevSection, listItem->attached->m_section, Qt::CaseInsensitive)
            && (sectionCriteria->labelPositioning() & QQuickViewSection::InlineLabels
                || (listItem->index == 0 && sectionCriteria->labelPositioning() & QQuickViewSection::CurrentLabelAtStart))) {
        if (!listItem->section()) {
            qreal pos = listItem->position();
            listItem->setSection(getSectionItem(listItem->attached->m_section));
            listItem->setPosition(pos);
        } else {
            QQmlContext *context = QQmlEngine::contextForObject(listItem->section())->parentContext();
            context->setContextProperty(QLatin1String("section"), listItem->attached->m_section);
        }
    } else if (listItem->section()) {
        qreal pos = listItem->position();
        releaseSectionItem(listItem->section());
        listItem->setSection(0);
        listItem->setPosition(pos);
    }
}

void QQuickListViewPrivate::updateStickySections()
{
    if (!sectionCriteria || !sectionCriteria->delegate()
            || (!sectionCriteria->labelPositioning() && !currentSectionItem && !nextSectionItem))
        return;

    bool isFlowReversed = isContentFlowReversed();
    qreal viewPos = isFlowReversed ? -position()-size() : position();
    qreal startPos = hasStickyHeader() ? header->endPosition() : viewPos;
    qreal endPos = hasStickyFooter() ? footer->position() : viewPos + size();

    QQuickItem *sectionItem = 0;
    QQuickItem *lastSectionItem = 0;
    int index = 0;
    while (index < visibleItems.count()) {
        if (QQuickItem *section = static_cast<FxListItemSG *>(visibleItems.at(index))->section()) {
            // Find the current section header and last visible section header
            // and hide them if they will overlap a static section header.
            qreal sectionPos = orient == QQuickListView::Vertical ? section->y() : section->x();
            qreal sectionSize = orient == QQuickListView::Vertical ? section->height() : section->width();
            bool visTop = true;
            if (sectionCriteria->labelPositioning() & QQuickViewSection::CurrentLabelAtStart)
                visTop = isFlowReversed ? -sectionPos-sectionSize >= startPos : sectionPos >= startPos;
            bool visBot = true;
            if (sectionCriteria->labelPositioning() & QQuickViewSection::NextLabelAtEnd)
                visBot = isFlowReversed ? -sectionPos <= endPos : sectionPos + sectionSize < endPos;
            section->setVisible(visBot && visTop);
            if (visTop && !sectionItem)
                sectionItem = section;
            if (isFlowReversed) {
               if (-sectionPos <= endPos)
                    lastSectionItem = section;
            } else {
                if (sectionPos + sectionSize < endPos)
                    lastSectionItem = section;
            }
        }
        ++index;
    }

    // Current section header
    if (sectionCriteria->labelPositioning() & QQuickViewSection::CurrentLabelAtStart && isValid() && visibleItems.count()) {
        if (!currentSectionItem) {
            currentSectionItem = getSectionItem(currentSection);
        } else if (QString::compare(currentStickySection, currentSection, Qt::CaseInsensitive)) {
            QQmlContext *context = QQmlEngine::contextForObject(currentSectionItem)->parentContext();
            context->setContextProperty(QLatin1String("section"), currentSection);
        }
        currentStickySection = currentSection;
        if (!currentSectionItem)
            return;

        qreal sectionSize = orient == QQuickListView::Vertical ? currentSectionItem->height() : currentSectionItem->width();
        bool atBeginning = orient == QQuickListView::Vertical ? (isBottomToTop() ? vData.atEnd : vData.atBeginning) : (isRightToLeft() ? hData.atEnd : hData.atBeginning);

        currentSectionItem->setVisible(!atBeginning && (!header || hasStickyHeader() || header->endPosition() < viewPos));
        qreal pos = isFlowReversed ? position() + size() - sectionSize : startPos;
        if (header)
            pos = isFlowReversed ? qMin(-header->endPosition() - sectionSize, pos) : qMax(header->endPosition(), pos);
        if (sectionItem) {
            qreal sectionPos = orient == QQuickListView::Vertical ? sectionItem->y() : sectionItem->x();
            pos = isFlowReversed ? qMax(pos, sectionPos + sectionSize) : qMin(pos, sectionPos - sectionSize);
        }
        if (footer)
            pos = isFlowReversed ? qMax(-footer->position(), pos) : qMin(footer->position() - sectionSize, pos);
        if (orient == QQuickListView::Vertical)
            currentSectionItem->setY(pos);
        else
            currentSectionItem->setX(pos);
    } else if (currentSectionItem) {
        releaseSectionItem(currentSectionItem);
        currentSectionItem = 0;
    }

    // Next section footer
    if (sectionCriteria->labelPositioning() & QQuickViewSection::NextLabelAtEnd && isValid() && visibleItems.count()) {
        if (!nextSectionItem) {
            nextSectionItem = getSectionItem(nextSection);
        } else if (QString::compare(nextStickySection, nextSection, Qt::CaseInsensitive)) {
            QQmlContext *context = QQmlEngine::contextForObject(nextSectionItem)->parentContext();
            context->setContextProperty(QLatin1String("section"), nextSection);
        }
        nextStickySection = nextSection;
        if (!nextSectionItem)
            return;

        qreal sectionSize = orient == QQuickListView::Vertical ? nextSectionItem->height() : nextSectionItem->width();
        nextSectionItem->setVisible(!nextSection.isEmpty());
        qreal pos = isFlowReversed ? position() : endPos - sectionSize;
        if (footer)
            pos = isFlowReversed ? qMax(-footer->position(), pos) : qMin(footer->position() - sectionSize, pos);
        if (lastSectionItem) {
            qreal sectionPos = orient == QQuickListView::Vertical ? lastSectionItem->y() : lastSectionItem->x();
            pos = isFlowReversed ? qMin(pos, sectionPos - sectionSize) : qMax(pos, sectionPos + sectionSize);
        }
        if (header)
            pos = isFlowReversed ? qMin(-header->endPosition() - sectionSize, pos) : qMax(header->endPosition(), pos);
        if (orient == QQuickListView::Vertical)
            nextSectionItem->setY(pos);
        else
            nextSectionItem->setX(pos);
    } else if (nextSectionItem) {
        releaseSectionItem(nextSectionItem);
        nextSectionItem = 0;
    }
}

void QQuickListViewPrivate::updateSections()
{
    Q_Q(QQuickListView);
    if (!q->isComponentComplete())
        return;

    QQuickItemViewPrivate::updateSections();

    if (sectionCriteria && !visibleItems.isEmpty() && isValid()) {
        QString prevSection;
        if (visibleIndex > 0)
            prevSection = sectionAt(visibleIndex-1);
        QQuickListViewAttached *prevAtt = 0;
        int prevIdx = -1;
        int idx = -1;
        for (int i = 0; i < visibleItems.count(); ++i) {
            FxViewItem *item = visibleItems.at(i);
            QQuickListViewAttached *attached = static_cast<QQuickListViewAttached*>(item->attached);
            attached->setPrevSection(prevSection);
            if (visibleItems.at(i)->index != -1) {
                QString propValue = model->stringValue(visibleItems.at(i)->index, sectionCriteria->property());
                attached->setSection(sectionCriteria->sectionString(propValue));
                idx = visibleItems.at(i)->index;
            }
            updateInlineSection(static_cast<FxListItemSG*>(visibleItems.at(i)));
            if (prevAtt)
                prevAtt->setNextSection(sectionAt(prevIdx+1));
            prevSection = attached->section();
            prevAtt = attached;
            prevIdx = item->index;
        }
        if (prevAtt) {
            if (idx > 0 && idx < model->count()-1)
                prevAtt->setNextSection(sectionAt(idx+1));
            else
                prevAtt->setNextSection(QString());
        }
    }

    lastVisibleSection = QString();
}

void QQuickListViewPrivate::updateCurrentSection()
{
    Q_Q(QQuickListView);
    if (!sectionCriteria || visibleItems.isEmpty()) {
        if (!currentSection.isEmpty()) {
            currentSection.clear();
            emit q->currentSectionChanged();
        }
        return;
    }
    bool inlineSections = sectionCriteria->labelPositioning() & QQuickViewSection::InlineLabels;
    qreal viewPos = isContentFlowReversed() ? -position()-size() : position();
    qreal startPos = hasStickyHeader() ? header->endPosition() : viewPos;
    int index = 0;
    int modelIndex = visibleIndex;
    while (index < visibleItems.count() && visibleItems.at(index)->endPosition() <= startPos) {
        if (visibleItems.at(index)->index != -1)
            modelIndex = visibleItems.at(index)->index;
        ++index;
    }

    QString newSection = currentSection;
    if (index < visibleItems.count())
        newSection = visibleItems.at(index)->attached->section();
    else
        newSection = (*visibleItems.constBegin())->attached->section();
    if (newSection != currentSection) {
        currentSection = newSection;
        updateStickySections();
        emit q->currentSectionChanged();
    }

    if (sectionCriteria->labelPositioning() & QQuickViewSection::NextLabelAtEnd) {
        // Don't want to scan for next section on every movement, so remember
        // the last section in the visible area and only scan for the next
        // section when that changes.  Clearing lastVisibleSection will also
        // force searching.
        QString lastSection = currentSection;
        qreal endPos = hasStickyFooter() ? footer->position() : viewPos + size();
        if (nextSectionItem && !inlineSections)
            endPos -= orient == QQuickListView::Vertical ? nextSectionItem->height() : nextSectionItem->width();
        while (index < visibleItems.count() && static_cast<FxListItemSG*>(visibleItems.at(index))->itemPosition() < endPos) {
            if (visibleItems.at(index)->index != -1)
                modelIndex = visibleItems.at(index)->index;
            lastSection = visibleItems.at(index)->attached->section();
            ++index;
        }

        if (lastVisibleSection != lastSection) {
            nextSection = QString();
            lastVisibleSection = lastSection;
            for (int i = modelIndex; i < itemCount; ++i) {
                QString section = sectionAt(i);
                if (section != lastSection) {
                    nextSection = section;
                    updateStickySections();
                    break;
                }
            }
        }
    }
}

void QQuickListViewPrivate::initializeCurrentItem()
{
    QQuickItemViewPrivate::initializeCurrentItem();

    if (currentItem) {
        FxListItemSG *listItem = static_cast<FxListItemSG *>(currentItem);

        // don't reposition the item if it is already in the visibleItems list
        FxViewItem *actualItem = visibleItem(currentIndex);
        if (!actualItem) {
            if (currentIndex == visibleIndex - 1 && visibleItems.count()) {
                // We can calculate exact postion in this case
                listItem->setPosition(visibleItems.constFirst()->position() - currentItem->size() - spacing);
            } else {
                // Create current item now and position as best we can.
                // Its position will be corrected when it becomes visible.
                listItem->setPosition(positionAt(currentIndex));
            }
        }

        if (visibleItems.isEmpty())
            averageSize = listItem->size();
    }
}

void QQuickListViewPrivate::updateAverage()
{
    if (!visibleItems.count())
        return;
    qreal sum = 0.0;
    for (int i = 0; i < visibleItems.count(); ++i)
        sum += visibleItems.at(i)->size();
    averageSize = qRound(sum / visibleItems.count());
}

qreal QQuickListViewPrivate::headerSize() const
{
    return header ? header->size() : 0.0;
}

qreal QQuickListViewPrivate::footerSize() const
{
    return footer ? footer->size() : 0.0;
}

bool QQuickListViewPrivate::showHeaderForIndex(int index) const
{
    return index == 0;
}

bool QQuickListViewPrivate::showFooterForIndex(int index) const
{
    return model && index == model->count()-1;
}

void QQuickListViewPrivate::updateFooter()
{
    Q_Q(QQuickListView);
    bool created = false;
    if (!footer) {
        QQuickItem *item = createComponentItem(footerComponent, 1.0);
        if (!item)
            return;
        footer = new FxListItemSG(item, q, true);
        footer->trackGeometry(true);
        created = true;
    }

    FxListItemSG *listItem = static_cast<FxListItemSG*>(footer);
    if (footerPositioning == QQuickListView::OverlayFooter) {
        listItem->setPosition(isContentFlowReversed() ? -position() - footerSize() : position() + size() - footerSize());
    } else if (visibleItems.count()) {
        if (footerPositioning == QQuickListView::PullBackFooter) {
            qreal viewPos = isContentFlowReversed() ? -position() : position() + size();
            qreal clampedPos = qBound(originPosition() - footerSize() + size(), listItem->position(), lastPosition());
            listItem->setPosition(qBound(viewPos - footerSize(), clampedPos, viewPos));
        } else {
            qreal endPos = lastPosition();
            if (findLastVisibleIndex() == model->count()-1) {
                listItem->setPosition(endPos);
            } else {
                qreal visiblePos = position() + q->height();
                if (endPos <= visiblePos || listItem->position() < endPos)
                    listItem->setPosition(endPos);
            }
        }
    } else {
        listItem->setPosition(visiblePos);
    }

    if (created)
        emit q->footerItemChanged();
}

void QQuickListViewPrivate::updateHeader()
{
    Q_Q(QQuickListView);
    bool created = false;
    if (!header) {
        QQuickItem *item = createComponentItem(headerComponent, 1.0);
        if (!item)
            return;
        header = new FxListItemSG(item, q, true);
        header->trackGeometry(true);
        created = true;
    }

    FxListItemSG *listItem = static_cast<FxListItemSG*>(header);
    if (headerPositioning == QQuickListView::OverlayHeader) {
        listItem->setPosition(isContentFlowReversed() ? -position() - size() : position());
    } else if (visibleItems.count()) {
        if (headerPositioning == QQuickListView::PullBackHeader) {
            qreal viewPos = isContentFlowReversed() ? -position() - size() : position();
            qreal clampedPos = qBound(originPosition() - headerSize(), listItem->position(), lastPosition() - headerSize() - size());
            listItem->setPosition(qBound(viewPos - headerSize(), clampedPos, viewPos));
        } else {
            qreal startPos = originPosition();
            if (visibleIndex == 0) {
                listItem->setPosition(startPos - headerSize());
            } else {
                if (position() <= startPos || listItem->position() > startPos - headerSize())
                    listItem->setPosition(startPos - headerSize());
            }
        }
    } else {
        listItem->setPosition(-headerSize());
    }

    if (created)
        emit q->headerItemChanged();
}

bool QQuickListViewPrivate::hasStickyHeader() const
{
    return header && headerPositioning != QQuickListView::InlineHeader;
}

bool QQuickListViewPrivate::hasStickyFooter() const
{
    return footer && footerPositioning != QQuickListView::InlineFooter;
}

void QQuickListViewPrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change,
                                                const QRectF &oldGeometry)
{
    Q_Q(QQuickListView);

    QQuickItemViewPrivate::itemGeometryChanged(item, change, oldGeometry);
    if (!q->isComponentComplete())
        return;

    if (currentItem && currentItem->item == item) {
        const bool contentFlowReversed = isContentFlowReversed();
        const qreal pos = position();
        const qreal sz = size();
        const qreal from = contentFlowReversed ? -pos - displayMarginBeginning - sz : pos - displayMarginBeginning;
        const qreal to = contentFlowReversed ? -pos + displayMarginEnd : pos + sz + displayMarginEnd;
        QQuickItemPrivate::get(currentItem->item)->setCulled(currentItem->endPosition() < from || currentItem->position() > to);
    }

    if (item != contentItem && (!highlight || item != highlight->item)) {
        if ((orient == QQuickListView::Vertical && change.heightChange())
            || (orient == QQuickListView::Horizontal && change.widthChange())) {

            // if visibleItems.first() has resized, adjust its pos since it is used to
            // position all subsequent items
            if (visibleItems.count() && item == visibleItems.constFirst()->item) {
                FxListItemSG *listItem = static_cast<FxListItemSG*>(visibleItems.constFirst());
                if (listItem->transitionScheduledOrRunning())
                    return;
                if (orient == QQuickListView::Vertical) {
                    const qreal oldItemEndPosition = verticalLayoutDirection == QQuickItemView::BottomToTop ? -oldGeometry.y() : oldGeometry.y() + oldGeometry.height();
                    const qreal heightDiff = item->height() - oldGeometry.height();
                    if (verticalLayoutDirection == QQuickListView::TopToBottom && oldItemEndPosition < q->contentY())
                        listItem->setPosition(listItem->position() - heightDiff, true);
                    else if (verticalLayoutDirection == QQuickListView::BottomToTop && oldItemEndPosition > q->contentY())
                        listItem->setPosition(listItem->position() + heightDiff, true);
                } else {
                    const qreal oldItemEndPosition = q->effectiveLayoutDirection() == Qt::RightToLeft ? -oldGeometry.x() : oldGeometry.x() + oldGeometry.width();
                    const qreal widthDiff = item->width() - oldGeometry.width();
                    if (q->effectiveLayoutDirection() == Qt::LeftToRight && oldItemEndPosition < q->contentX())
                        listItem->setPosition(listItem->position() - widthDiff, true);
                    else if (q->effectiveLayoutDirection() == Qt::RightToLeft && oldItemEndPosition > q->contentX())
                        listItem->setPosition(listItem->position() + widthDiff, true);
                }
            }
            forceLayoutPolish();
        }
    }
}

void QQuickListViewPrivate::fixupPosition()
{
    if ((haveHighlightRange && highlightRange == QQuickListView::StrictlyEnforceRange)
        || snapMode != QQuickListView::NoSnap)
        moveReason = Other;
    if (orient == QQuickListView::Vertical)
        fixupY();
    else
        fixupX();
}

void QQuickListViewPrivate::fixup(AxisData &data, qreal minExtent, qreal maxExtent)
{
    if ((orient == QQuickListView::Horizontal && &data == &vData)
        || (orient == QQuickListView::Vertical && &data == &hData))
        return;

    correctFlick = false;
    fixupMode = moveReason == Mouse ? fixupMode : Immediate;
    bool strictHighlightRange = haveHighlightRange && highlightRange == QQuickListView::StrictlyEnforceRange;

    qreal viewPos = isContentFlowReversed() ? -position()-size() : position();

    if (snapMode != QQuickListView::NoSnap && moveReason != QQuickListViewPrivate::SetIndex) {
        qreal tempPosition = isContentFlowReversed() ? -position()-size() : position();
        if (snapMode == QQuickListView::SnapOneItem && moveReason == Mouse) {
            // if we've been dragged < averageSize/2 then bias towards the next item
            qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
            qreal bias = 0;
            if (data.velocity > 0 && dist > QML_FLICK_SNAPONETHRESHOLD && dist < averageSize/2)
                bias = averageSize/2;
            else if (data.velocity < 0 && dist < -QML_FLICK_SNAPONETHRESHOLD && dist > -averageSize/2)
                bias = -averageSize/2;
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
            if (topItem->index == 0 && header && tempPosition+highlightRangeStart < header->position()+header->size()/2 && !strictHighlightRange) {
                pos = isContentFlowReversed() ? - header->position() + highlightRangeStart - size() : header->position() - highlightRangeStart;
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
    } else if (currentItem && strictHighlightRange && moveReason != QQuickListViewPrivate::SetIndex) {
        updateHighlight();
        qreal pos = static_cast<FxListItemSG*>(currentItem)->itemPosition();
        if (viewPos < pos + static_cast<FxListItemSG*>(currentItem)->itemSize() - highlightRangeEnd)
            viewPos = pos + static_cast<FxListItemSG*>(currentItem)->itemSize() - highlightRangeEnd;
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
    } else {
        QQuickItemViewPrivate::fixup(data, minExtent, maxExtent);
    }
    data.inOvershoot = false;
    fixupMode = Normal;
}

bool QQuickListViewPrivate::flick(AxisData &data, qreal minExtent, qreal maxExtent, qreal vSize,
                                        QQuickTimeLineCallback::Callback fixupCallback, qreal velocity)
{
    data.fixingUp = false;
    moveReason = Mouse;
    if ((!haveHighlightRange || highlightRange != QQuickListView::StrictlyEnforceRange) && snapMode == QQuickListView::NoSnap) {
        correctFlick = true;
        return QQuickItemViewPrivate::flick(data, minExtent, maxExtent, vSize, fixupCallback, velocity);
    }
    qreal maxDistance = 0;
    qreal dataValue = isContentFlowReversed() ? -data.move.value()+size() : data.move.value();

    // -ve velocity means list is moving up/left
    if (velocity > 0) {
        if (data.move.value() < minExtent) {
            if (snapMode == QQuickListView::SnapOneItem && !hData.flicking && !vData.flicking) {
                // if we've been dragged < averageSize/2 then bias towards the next item
                qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
                qreal bias = dist < averageSize/2 ? averageSize/2 : 0;
                if (isContentFlowReversed())
                    bias = -bias;
                data.flickTarget = -snapPosAt(-(dataValue - highlightRangeStart) - bias) + highlightRangeStart;
                maxDistance = qAbs(data.flickTarget - data.move.value());
                velocity = maxVelocity;
            } else {
                maxDistance = qAbs(minExtent - data.move.value());
            }
        }
        if (snapMode == QQuickListView::NoSnap && highlightRange != QQuickListView::StrictlyEnforceRange)
            data.flickTarget = minExtent;
    } else {
        if (data.move.value() > maxExtent) {
            if (snapMode == QQuickListView::SnapOneItem && !hData.flicking && !vData.flicking) {
                // if we've been dragged < averageSize/2 then bias towards the next item
                qreal dist = data.move.value() - (data.pressPos - data.dragStartOffset);
                qreal bias = -dist < averageSize/2 ? averageSize/2 : 0;
                if (isContentFlowReversed())
                    bias = -bias;
                data.flickTarget = -snapPosAt(-(dataValue - highlightRangeStart) + bias) + highlightRangeStart;
                maxDistance = qAbs(data.flickTarget - data.move.value());
                velocity = -maxVelocity;
            } else {
                maxDistance = qAbs(maxExtent - data.move.value());
            }
        }
        if (snapMode == QQuickListView::NoSnap && highlightRange != QQuickListView::StrictlyEnforceRange)
            data.flickTarget = maxExtent;
    }
    bool overShoot = boundsBehavior & QQuickFlickable::OvershootBounds;
    if (maxDistance > 0 || overShoot) {
        // These modes require the list to stop exactly on an item boundary.
        // The initial flick will estimate the boundary to stop on.
        // Since list items can have variable sizes, the boundary will be
        // reevaluated and adjusted as we approach the boundary.
        qreal v = velocity;
        if (maxVelocity != -1 && maxVelocity < qAbs(v)) {
            if (v < 0)
                v = -maxVelocity;
            else
                v = maxVelocity;
        }
        if (!hData.flicking && !vData.flicking) {
            // the initial flick - estimate boundary
            qreal accel = deceleration;
            qreal v2 = v * v;
            overshootDist = 0.0;
            // + averageSize/4 to encourage moving at least one item in the flick direction
            qreal dist = v2 / (accel * 2.0) + averageSize/4;
            if (maxDistance > 0)
                dist = qMin(dist, maxDistance);
            if (v > 0)
                dist = -dist;
            if ((maxDistance > 0.0 && v2 / (2.0f * maxDistance) < accel) || snapMode == QQuickListView::SnapOneItem) {
                if (snapMode != QQuickListView::SnapOneItem) {
                    qreal distTemp = isContentFlowReversed() ? -dist : dist;
                    data.flickTarget = -snapPosAt(-(dataValue - highlightRangeStart) + distTemp) + highlightRangeStart;
                }
                data.flickTarget = isContentFlowReversed() ? -data.flickTarget+size() : data.flickTarget;
                if (overShoot) {
                    if (data.flickTarget > minExtent) {
                        overshootDist = overShootDistance(vSize);
                        data.flickTarget += overshootDist;
                    } else if (data.flickTarget < maxExtent) {
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
            } else if (overShoot) {
                data.flickTarget = data.move.value() - dist;
                if (data.flickTarget > minExtent) {
                    overshootDist = overShootDistance(vSize);
                    data.flickTarget += overshootDist;
                } else if (data.flickTarget < maxExtent) {
                    overshootDist = overShootDistance(vSize);
                    data.flickTarget -= overshootDist;
                }
            }
            timeline.reset(data.move);
            timeline.accel(data.move, v, accel, maxDistance + overshootDist);
            timeline.callback(QQuickTimeLineCallback(&data.move, fixupCallback, this));
            correctFlick = true;
            return true;
        } else {
            // reevaluate the target boundary.
            qreal newtarget = data.flickTarget;
            if (snapMode != QQuickListView::NoSnap || highlightRange == QQuickListView::StrictlyEnforceRange) {
                qreal tempFlickTarget = isContentFlowReversed() ? -data.flickTarget+size() : data.flickTarget;
                newtarget = -snapPosAt(-(tempFlickTarget - highlightRangeStart)) + highlightRangeStart;
                newtarget = isContentFlowReversed() ? -newtarget+size() : newtarget;
            }
            if (velocity < 0 && newtarget <= maxExtent)
                newtarget = maxExtent - overshootDist;
            else if (velocity > 0 && newtarget >= minExtent)
                newtarget = minExtent + overshootDist;
            if (newtarget == data.flickTarget) { // boundary unchanged - nothing to do
                if (qAbs(velocity) < MinimumFlickVelocity)
                    correctFlick = false;
                return false;
            }
            data.flickTarget = newtarget;
            qreal dist = -newtarget + data.move.value();
            if ((v < 0 && dist < 0) || (v > 0 && dist > 0)) {
                correctFlick = false;
                timeline.reset(data.move);
                fixup(data, minExtent, maxExtent);
                return false;
            }
            timeline.reset(data.move);
            timeline.accelDistance(data.move, v, -dist);
            timeline.callback(QQuickTimeLineCallback(&data.move, fixupCallback, this));
            return false;
        }
    } else {
        correctFlick = false;
        timeline.reset(data.move);
        fixup(data, minExtent, maxExtent);
        return false;
    }
}

//----------------------------------------------------------------------------

/*!
    \qmltype ListView
    \instantiates QQuickListView
    \inqmlmodule QtQuick
    \ingroup qtquick-views
    \inherits Flickable
    \brief Provides a list view of items provided by a model

    A ListView displays data from models created from built-in QML types like ListModel
    and XmlListModel, or custom model classes defined in C++ that inherit from
    QAbstractItemModel or QAbstractListModel.

    A ListView has a \l model, which defines the data to be displayed, and
    a \l delegate, which defines how the data should be displayed. Items in a
    ListView are laid out horizontally or vertically. List views are inherently
    flickable because ListView inherits from \l Flickable.

    \section1 Example Usage

    The following example shows the definition of a simple list model defined
    in a file called \c ContactModel.qml:

    \snippet qml/listview/ContactModel.qml 0

    Another component can display this model data in a ListView, like this:

    \snippet qml/listview/listview.qml import
    \codeline
    \snippet qml/listview/listview.qml classdocs simple

    \image listview-simple.png

    Here, the ListView creates a \c ContactModel component for its model, and a \l Text item
    for its delegate. The view will create a new \l Text component for each item in the model. Notice
    the delegate is able to access the model's \c name and \c number data directly.

    An improved list view is shown below. The delegate is visually improved and is moved
    into a separate \c contactDelegate component.

    \snippet qml/listview/listview.qml classdocs advanced
    \image listview-highlight.png

    The currently selected item is highlighted with a blue \l Rectangle using the \l highlight property,
    and \c focus is set to \c true to enable keyboard navigation for the list view.
    The list view itself is a focus scope (see \l{Keyboard Focus in Qt Quick} for more details).

    Delegates are instantiated as needed and may be destroyed at any time.
    They are parented to ListView's \l {Flickable::contentItem}{contentItem}, not to the view itself.
    State should \e never be stored in a delegate.

    ListView attaches a number of properties to the root item of the delegate, for example
    \c ListView.isCurrentItem.  In the following example, the root delegate item can access
    this attached property directly as \c ListView.isCurrentItem, while the child
    \c contactInfo object must refer to this property as \c wrapper.ListView.isCurrentItem.

    \snippet qml/listview/listview.qml isCurrentItem

    \note Views do not enable \e clip automatically.  If the view
    is not clipped by another item or the screen, it will be necessary
    to set \e {clip: true} in order to have the out of view items clipped
    nicely.


    \section1 ListView Layouts

    The layout of the items in a ListView can be controlled by these properties:

    \list
    \li \l orientation - controls whether items flow horizontally or vertically.
        This value can be either Qt.Horizontal or Qt.Vertical.
    \li \l layoutDirection - controls the horizontal layout direction for a
        horizontally-oriented view: that is, whether items are laid out from the left side of
        the view to the right, or vice-versa. This value can be either Qt.LeftToRight or Qt.RightToLeft.
    \li \l verticalLayoutDirection - controls the vertical layout direction for a vertically-oriented
        view: that is, whether items are laid out from the top of the view down towards the bottom of
        the view, or vice-versa. This value can be either ListView.TopToBottom or ListView.BottomToTop.
    \endlist

    By default, a ListView has a vertical orientation, and items are laid out from top to bottom. The
    table below shows the different layouts that a ListView can have, depending on the values of
    the properties listed above.

    \table
    \header
        \li {2, 1}
            \b ListViews with Qt.Vertical orientation
    \row
        \li Top to bottom
            \image listview-layout-toptobottom.png
        \li Bottom to top
            \image listview-layout-bottomtotop.png
    \header
        \li {2, 1}
            \b ListViews with Qt.Horizontal orientation
    \row
        \li Left to right
            \image listview-layout-lefttoright.png
        \li Right to left
            \image listview-layout-righttoleft.png
    \endtable

    \sa {QML Data Models}, GridView, PathView, {Qt Quick Examples - Views}
*/
QQuickListView::QQuickListView(QQuickItem *parent)
    : QQuickItemView(*(new QQuickListViewPrivate), parent)
{
}

QQuickListView::~QQuickListView()
{
}

/*!
    \qmlattachedproperty bool QtQuick::ListView::isCurrentItem
    This attached property is true if this delegate is the current item; otherwise false.

    It is attached to each instance of the delegate.

    This property may be used to adjust the appearance of the current item, for example:

    \snippet qml/listview/listview.qml isCurrentItem
*/

/*!
    \qmlattachedproperty ListView QtQuick::ListView::view
    This attached property holds the view that manages this delegate instance.

    It is attached to each instance of the delegate and also to the header, the footer,
    the section and the highlight delegates.
*/

/*!
    \qmlattachedproperty string QtQuick::ListView::previousSection
    This attached property holds the section of the previous element.

    It is attached to each instance of the delegate.

    The section is evaluated using the \l {ListView::section.property}{section} properties.
*/

/*!
    \qmlattachedproperty string QtQuick::ListView::nextSection
    This attached property holds the section of the next element.

    It is attached to each instance of the delegate.

    The section is evaluated using the \l {ListView::section.property}{section} properties.
*/

/*!
    \qmlattachedproperty string QtQuick::ListView::section
    This attached property holds the section of this element.

    It is attached to each instance of the delegate.

    The section is evaluated using the \l {ListView::section.property}{section} properties.
*/

/*!
    \qmlattachedproperty bool QtQuick::ListView::delayRemove

    This attached property holds whether the delegate may be destroyed. It
    is attached to each instance of the delegate. The default value is false.

    It is sometimes necessary to delay the destruction of an item
    until an animation completes. The example delegate below ensures that the
    animation completes before the item is removed from the list.

    \snippet qml/listview/listview.qml delayRemove

    If a \l remove transition has been specified, it will not be applied until
    delayRemove is returned to \c false.
*/

/*!
    \qmlattachedsignal QtQuick::ListView::add()
    This attached signal is emitted immediately after an item is added to the view.

    If an \l add transition is specified, it is applied immediately after
    this signal is handled.

    The corresponding handler is \c onAdd.
*/

/*!
    \qmlattachedsignal QtQuick::ListView::remove()
    This attached signal is emitted immediately before an item is removed from the view.

    If a \l remove transition has been specified, it is applied after
    this signal is handled, providing that \l delayRemove is false.

    The corresponding handler is \c onRemove.
*/

/*!
    \qmlproperty model QtQuick::ListView::model
    This property holds the model providing data for the list.

    The model provides the set of data that is used to create the items
    in the view. Models can be created directly in QML using \l ListModel, \l XmlListModel
    or \l VisualItemModel, or provided by C++ model classes. If a C++ model class is
    used, it must be a subclass of \l QAbstractItemModel or a simple list.

    \sa {qml-data-models}{Data Models}
*/

/*!
    \qmlproperty Component QtQuick::ListView::delegate

    The delegate provides a template defining each item instantiated by the view.
    The index is exposed as an accessible \c index property.  Properties of the
    model are also available depending upon the type of \l {qml-data-models}{Data Model}.

    The number of objects and bindings in the delegate has a direct effect on the
    flicking performance of the view.  If at all possible, place functionality
    that is not needed for the normal display of the delegate in a \l Loader which
    can load additional components when needed.

    The ListView will lay out the items based on the size of the root item
    in the delegate.

    It is recommended that the delegate's size be a whole number to avoid sub-pixel
    alignment of items.

    The default \l {QQuickItem::z}{stacking order} of delegate instances is \c 1.

    \note Delegates are instantiated as needed and may be destroyed at any time.
    They are parented to ListView's \l {Flickable::contentItem}{contentItem}, not to the view itself.
    State should \e never be stored in a delegate.
*/
/*!
    \qmlproperty int QtQuick::ListView::currentIndex
    \qmlproperty Item QtQuick::ListView::currentItem

    The \c currentIndex property holds the index of the current item, and
    \c currentItem holds the current item.   Setting the currentIndex to -1
    will clear the highlight and set currentItem to null.

    If highlightFollowsCurrentItem is \c true, setting either of these
    properties will smoothly scroll the ListView so that the current
    item becomes visible.

    Note that the position of the current item
    may only be approximate until it becomes visible in the view.
*/

/*!
  \qmlproperty Item QtQuick::ListView::highlightItem

    This holds the highlight item created from the \l highlight component.

  The \c highlightItem is managed by the view unless
  \l highlightFollowsCurrentItem is set to false.
  The default \l {QQuickItem::z}{stacking order}
  of the highlight item is \c 0.

  \sa highlight, highlightFollowsCurrentItem
*/

/*!
  \qmlproperty int QtQuick::ListView::count
  This property holds the number of items in the view.
*/

/*!
    \qmlproperty Component QtQuick::ListView::highlight
    This property holds the component to use as the highlight.

    An instance of the highlight component is created for each list.
    The geometry of the resulting component instance is managed by the list
    so as to stay with the current item, unless the highlightFollowsCurrentItem
    property is false. The default \l {QQuickItem::z}{stacking order} of the
    highlight item is \c 0.

    \sa highlightItem, highlightFollowsCurrentItem,
    {Qt Quick Examples - Views#Highlight}{ListView highlight example}
*/

/*!
    \qmlproperty bool QtQuick::ListView::highlightFollowsCurrentItem
    This property holds whether the highlight is managed by the view.

    If this property is true (the default value), the highlight is moved smoothly
    to follow the current item.  Otherwise, the
    highlight is not moved by the view, and any movement must be implemented
    by the highlight.

    Here is a highlight with its motion defined by a \l {SpringAnimation} item:

    \snippet qml/listview/listview.qml highlightFollowsCurrentItem

    Note that the highlight animation also affects the way that the view
    is scrolled.  This is because the view moves to maintain the
    highlight within the preferred highlight range (or visible viewport).

    \sa highlight, highlightMoveVelocity
*/
//###Possibly rename these properties, since they are very useful even without a highlight?
/*!
    \qmlproperty real QtQuick::ListView::preferredHighlightBegin
    \qmlproperty real QtQuick::ListView::preferredHighlightEnd
    \qmlproperty enumeration QtQuick::ListView::highlightRangeMode

    These properties define the preferred range of the highlight (for the current item)
    within the view. The \c preferredHighlightBegin value must be less than the
    \c preferredHighlightEnd value.

    These properties affect the position of the current item when the list is scrolled.
    For example, if the currently selected item should stay in the middle of the
    list when the view is scrolled, set the \c preferredHighlightBegin and
    \c preferredHighlightEnd values to the top and bottom coordinates of where the middle
    item would be. If the \c currentItem is changed programmatically, the list will
    automatically scroll so that the current item is in the middle of the view.
    Furthermore, the behavior of the current item index will occur whether or not a
    highlight exists.

    Valid values for \c highlightRangeMode are:

    \list
    \li ListView.ApplyRange - the view attempts to maintain the highlight within the range.
       However, the highlight can move outside of the range at the ends of the list or due
       to mouse interaction.
    \li ListView.StrictlyEnforceRange - the highlight never moves outside of the range.
       The current item changes if a keyboard or mouse action would cause the highlight to move
       outside of the range.
    \li ListView.NoHighlightRange - this is the default value.
    \endlist
*/
void QQuickListView::setHighlightFollowsCurrentItem(bool autoHighlight)
{
    Q_D(QQuickListView);
    if (d->autoHighlight != autoHighlight) {
        if (!autoHighlight) {
            if (d->highlightPosAnimator)
                d->highlightPosAnimator->stop();
            if (d->highlightWidthAnimator)
                d->highlightWidthAnimator->stop();
            if (d->highlightHeightAnimator)
                d->highlightHeightAnimator->stop();
        }
        QQuickItemView::setHighlightFollowsCurrentItem(autoHighlight);
    }
}

/*!
    \qmlproperty real QtQuick::ListView::spacing

    This property holds the spacing between items.

    The default value is 0.
*/
qreal QQuickListView::spacing() const
{
    Q_D(const QQuickListView);
    return d->spacing;
}

void QQuickListView::setSpacing(qreal spacing)
{
    Q_D(QQuickListView);
    if (spacing != d->spacing) {
        d->spacing = spacing;
        d->forceLayoutPolish();
        emit spacingChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::ListView::orientation
    This property holds the orientation of the list.

    Possible values:

    \list
    \li ListView.Horizontal - Items are laid out horizontally
    \li ListView.Vertical (default) - Items are laid out vertically
    \endlist

    \table
    \row
    \li Horizontal orientation:
    \image ListViewHorizontal.png

    \row
    \li Vertical orientation:
    \image listview-highlight.png
    \endtable
*/
QQuickListView::Orientation QQuickListView::orientation() const
{
    Q_D(const QQuickListView);
    return d->orient;
}

void QQuickListView::setOrientation(QQuickListView::Orientation orientation)
{
    Q_D(QQuickListView);
    if (d->orient != orientation) {
        d->orient = orientation;
        if (d->orient == Vertical) {
            setContentWidth(-1);
            setFlickableDirection(VerticalFlick);
            setContentX(0);
        } else {
            setContentHeight(-1);
            setFlickableDirection(HorizontalFlick);
            setContentY(0);
        }
        d->regenerate(true);
        emit orientationChanged();
    }
}

/*!
  \qmlproperty enumeration QtQuick::ListView::layoutDirection
  This property holds the layout direction of a horizontally-oriented list.

  Possible values:

  \list
  \li Qt.LeftToRight (default) - Items will be laid out from left to right.
  \li Qt.RightToLeft - Items will be laid out from right to let.
  \endlist

  Setting this property has no effect if the \l orientation is Qt.Vertical.

  \sa ListView::effectiveLayoutDirection, ListView::verticalLayoutDirection
*/


/*!
    \qmlproperty enumeration QtQuick::ListView::effectiveLayoutDirection
    This property holds the effective layout direction of a horizontally-oriented list.

    When using the attached property \l {LayoutMirroring::enabled}{LayoutMirroring::enabled} for locale layouts,
    the visual layout direction of the horizontal list will be mirrored. However, the
    property \l {ListView::layoutDirection}{layoutDirection} will remain unchanged.

    \sa ListView::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/


/*!
  \qmlproperty enumeration QtQuick::ListView::verticalLayoutDirection
  This property holds the layout direction of a vertically-oriented list.

  Possible values:

  \list
  \li ListView.TopToBottom (default) - Items are laid out from the top of the view down to the bottom of the view.
  \li ListView.BottomToTop - Items are laid out from the bottom of the view up to the top of the view.
  \endlist

  Setting this property has no effect if the \l orientation is Qt.Horizontal.

  \sa ListView::layoutDirection
*/


/*!
    \qmlproperty bool QtQuick::ListView::keyNavigationWraps
    This property holds whether the list wraps key navigation.

    If this is true, key navigation that would move the current item selection
    past the end of the list instead wraps around and moves the selection to
    the start of the list, and vice-versa.

    By default, key navigation is not wrapped.
*/

/*!
    \qmlproperty bool QtQuick::ListView::keyNavigationEnabled
    \since 5.7

    This property holds whether the key navigation of the list is enabled.

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
    \qmlproperty int QtQuick::ListView::cacheBuffer
    This property determines whether delegates are retained outside the
    visible area of the view.

    If this value is greater than zero, the view may keep as many delegates
    instantiated as it can fit within the buffer specified.  For example,
    if in a vertical view the delegate is 20 pixels high and \c cacheBuffer is
    set to 40, then up to 2 delegates above and 2 delegates below the visible
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
    \qmlproperty int QtQuick::ListView::displayMarginBeginning
    \qmlproperty int QtQuick::ListView::displayMarginEnd
    \since QtQuick 2.3

    This property allows delegates to be displayed outside of the view geometry.

    If this value is non-zero, the view will create extra delegates before the
    start of the view, or after the end. The view will create as many delegates
    as it can fit into the pixel size specified.

    For example, if in a vertical view the delegate is 20 pixels high and
    \c displayMarginBeginning and \c displayMarginEnd are both set to 40,
    then 2 delegates above and 2 delegates below will be created and shown.

    The default value is 0.

    This property is meant for allowing certain UI configurations,
    and not as a performance optimization. If you wish to create delegates
    outside of the view geometry for performance reasons, you probably
    want to use the cacheBuffer property instead.
*/

/*!
    \qmlpropertygroup QtQuick::ListView::section
    \qmlproperty string QtQuick::ListView::section.property
    \qmlproperty enumeration QtQuick::ListView::section.criteria
    \qmlproperty Component QtQuick::ListView::section.delegate
    \qmlproperty enumeration QtQuick::ListView::section.labelPositioning

    These properties determine the expression to be evaluated and appearance
    of the section labels.

    \c section.property holds the name of the property that is the basis
    of each section.

    \c section.criteria holds the criteria for forming each section based on
    \c section.property. This value can be one of:

    \list
    \li ViewSection.FullString (default) - sections are created based on the
    \c section.property value.
    \li ViewSection.FirstCharacter - sections are created based on the first
    character of the \c section.property value (for example, 'A', 'B', 'C'
    sections, etc. for an address book)
    \endlist

    A case insensitive comparison is used when determining section
    boundaries.

    \c section.delegate holds the delegate component for each section. The
    default \l {QQuickItem::z}{stacking order} of section delegate instances
    is \c 2.

    \c section.labelPositioning determines whether the current and/or
    next section labels stick to the start/end of the view, and whether
    the labels are shown inline.  This value can be a combination of:

    \list
    \li ViewSection.InlineLabels - section labels are shown inline between
    the item delegates separating sections (default).
    \li ViewSection.CurrentLabelAtStart - the current section label sticks to the
    start of the view as it is moved.
    \li ViewSection.NextLabelAtEnd - the next section label (beyond all visible
    sections) sticks to the end of the view as it is moved. \note Enabling
    \c ViewSection.NextLabelAtEnd requires the view to scan ahead for the next
    section, which has performance implications, especially for slower models.
    \endlist

    Each item in the list has attached properties named \c ListView.section,
    \c ListView.previousSection and \c ListView.nextSection.

    For example, here is a ListView that displays a list of animals, separated
    into sections. Each item in the ListView is placed in a different section
    depending on the "size" property of the model item. The \c sectionHeading
    delegate component provides the light blue bar that marks the beginning of
    each section.


    \snippet views/listview/sections.qml 0

    \image qml-listview-sections-example.png

    \note Adding sections to a ListView does not automatically re-order the
    list items by the section criteria.
    If the model is not ordered by section, then it is possible that
    the sections created will not be unique; each boundary between
    differing sections will result in a section header being created
    even if that section exists elsewhere.

    \sa {Qt Quick Examples - Views}{ListView examples}
*/
QQuickViewSection *QQuickListView::sectionCriteria()
{
    Q_D(QQuickListView);
    if (!d->sectionCriteria)
        d->sectionCriteria = new QQuickViewSection(this);
    return d->sectionCriteria;
}

/*!
    \qmlproperty string QtQuick::ListView::currentSection
    This property holds the section that is currently at the beginning of the view.
*/
QString QQuickListView::currentSection() const
{
    Q_D(const QQuickListView);
    return d->currentSection;
}

/*!
    \qmlproperty real QtQuick::ListView::highlightMoveVelocity
    \qmlproperty int QtQuick::ListView::highlightMoveDuration
    \qmlproperty real QtQuick::ListView::highlightResizeVelocity
    \qmlproperty int QtQuick::ListView::highlightResizeDuration

    These properties control the speed of the move and resize animations for the
    highlight delegate.

    \l highlightFollowsCurrentItem must be true for these properties
    to have effect.

    The default value for the velocity properties is 400 pixels/second.
    The default value for the duration properties is -1, i.e. the
    highlight will take as much time as necessary to move at the set speed.

    These properties have the same characteristics as a SmoothedAnimation.

    \sa highlightFollowsCurrentItem
*/
qreal QQuickListView::highlightMoveVelocity() const
{
    Q_D(const QQuickListView);
    return d->highlightMoveVelocity;
}

void QQuickListView::setHighlightMoveVelocity(qreal speed)
{
    Q_D(QQuickListView);
    if (d->highlightMoveVelocity != speed) {
        d->highlightMoveVelocity = speed;
        if (d->highlightPosAnimator)
            d->highlightPosAnimator->velocity = d->highlightMoveVelocity;
        emit highlightMoveVelocityChanged();
    }
}

void QQuickListView::setHighlightMoveDuration(int duration)
{
    Q_D(QQuickListView);
    if (d->highlightMoveDuration != duration) {
        if (d->highlightPosAnimator)
            d->highlightPosAnimator->userDuration = duration;
        QQuickItemView::setHighlightMoveDuration(duration);
    }
}

qreal QQuickListView::highlightResizeVelocity() const
{
    Q_D(const QQuickListView);
    return d->highlightResizeVelocity;
}

void QQuickListView::setHighlightResizeVelocity(qreal speed)
{
    Q_D(QQuickListView);
    if (d->highlightResizeVelocity != speed) {
        d->highlightResizeVelocity = speed;
        if (d->highlightWidthAnimator)
            d->highlightWidthAnimator->velocity = d->highlightResizeVelocity;
        if (d->highlightHeightAnimator)
            d->highlightHeightAnimator->velocity = d->highlightResizeVelocity;
        emit highlightResizeVelocityChanged();
    }
}

int QQuickListView::highlightResizeDuration() const
{
    Q_D(const QQuickListView);
    return d->highlightResizeDuration;
}

void QQuickListView::setHighlightResizeDuration(int duration)
{
    Q_D(QQuickListView);
    if (d->highlightResizeDuration != duration) {
        d->highlightResizeDuration = duration;
        if (d->highlightWidthAnimator)
            d->highlightWidthAnimator->userDuration = d->highlightResizeDuration;
        if (d->highlightHeightAnimator)
            d->highlightHeightAnimator->userDuration = d->highlightResizeDuration;
        emit highlightResizeDurationChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::ListView::snapMode

    This property determines how the view scrolling will settle following a drag or flick.
    The possible values are:

    \list
    \li ListView.NoSnap (default) - the view stops anywhere within the visible area.
    \li ListView.SnapToItem - the view settles with an item aligned with the start of
    the view.
    \li ListView.SnapOneItem - the view settles no more than one item away from the first
    visible item at the time the mouse button is released.  This mode is particularly
    useful for moving one page at a time.
    \endlist

    \c snapMode does not affect the \l currentIndex.  To update the
    \l currentIndex as the list is moved, set \l highlightRangeMode
    to \c ListView.StrictlyEnforceRange.

    \sa highlightRangeMode
*/
QQuickListView::SnapMode QQuickListView::snapMode() const
{
    Q_D(const QQuickListView);
    return d->snapMode;
}

void QQuickListView::setSnapMode(SnapMode mode)
{
    Q_D(QQuickListView);
    if (d->snapMode != mode) {
        d->snapMode = mode;
        emit snapModeChanged();
        d->fixupPosition();
    }
}


/*!
    \qmlproperty Component QtQuick::ListView::footer
    This property holds the component to use as the footer.

    An instance of the footer component is created for each view.  The
    footer is positioned at the end of the view, after any items. The
    default \l {QQuickItem::z}{stacking order} of the footer is \c 1.

    \sa header, footerItem
*/


/*!
    \qmlproperty Component QtQuick::ListView::header
    This property holds the component to use as the header.

    An instance of the header component is created for each view.  The
    header is positioned at the beginning of the view, before any items.
    The default \l {QQuickItem::z}{stacking order} of the header is \c 1.

    \sa footer, headerItem
*/

/*!
    \qmlproperty Item QtQuick::ListView::headerItem
    This holds the header item created from the \l header component.

    An instance of the header component is created for each view.  The
    header is positioned at the beginning of the view, before any items.
    The default \l {QQuickItem::z}{stacking order} of the header is \c 1.

    \sa header, footerItem
*/

/*!
    \qmlproperty Item QtQuick::ListView::footerItem
    This holds the footer item created from the \l footer component.

    An instance of the footer component is created for each view.  The
    footer is positioned at the end of the view, after any items. The
    default \l {QQuickItem::z}{stacking order} of the footer is \c 1.

    \sa footer, headerItem
*/

/*!
    \qmlproperty enumeration QtQuick::ListView::headerPositioning
    \since Qt 5.4

    This property determines the positioning of the \l{headerItem}{header item}.

    The possible values are:
    \list
    \li ListView.InlineHeader (default) - the header is positioned in the beginning
    of the content and moves together with the content like an ordinary item.
    \li ListView.OverlayHeader - the header is positioned in the beginning of the view.
    \li ListView.PullBackHeader - the header is positioned in the beginning of the view.
    The header can be pushed away by moving the content forwards, and pulled back by
    moving the content backwards.
    \endlist
*/
QQuickListView::HeaderPositioning QQuickListView::headerPositioning() const
{
    Q_D(const QQuickListView);
    return d->headerPositioning;
}

void QQuickListView::setHeaderPositioning(QQuickListView::HeaderPositioning positioning)
{
    Q_D(QQuickListView);
    if (d->headerPositioning != positioning) {
        d->applyPendingChanges();
        d->headerPositioning = positioning;
        if (isComponentComplete()) {
            d->updateHeader();
            d->updateViewport();
            d->fixupPosition();
        }
        emit headerPositioningChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::ListView::footerPositioning
    \since Qt 5.4

    This property determines the positioning of the \l{footerItem}{footer item}.

    The possible values are:
    \list
    \li ListView.InlineFooter (default) - the footer is positioned in the end
    of the content and moves together with the content like an ordinary item.
    \li ListView.OverlayFooter - the footer is positioned in the end of the view.
    \li ListView.PullBackFooter - the footer is positioned in the end of the view.
    The footer can be pushed away by moving the content backwards, and pulled back by
    moving the content forwards.
    \endlist
*/
QQuickListView::FooterPositioning QQuickListView::footerPositioning() const
{
    Q_D(const QQuickListView);
    return d->footerPositioning;
}

void QQuickListView::setFooterPositioning(QQuickListView::FooterPositioning positioning)
{
    Q_D(QQuickListView);
    if (d->footerPositioning != positioning) {
        d->applyPendingChanges();
        d->footerPositioning = positioning;
        if (isComponentComplete()) {
            d->updateFooter();
            d->updateViewport();
            d->fixupPosition();
        }
        emit footerPositioningChanged();
    }
}

/*!
    \qmlproperty Transition QtQuick::ListView::populate

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
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::add

    This property holds the transition to apply to items that are added to the view.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::addDisplaced

    This property holds the transition to apply to items within the view that are displaced by
    the addition of other items to the view.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::move

    This property holds the transition to apply to items in the view that are being moved due
    to a move operation in the view's \l model.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::moveDisplaced

    This property holds the transition to apply to items that are displaced by a move operation in
    the view's \l model.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::remove

    This property holds the transition to apply to items that are removed from the view.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::removeDisplaced

    This property holds the transition to apply to items in the view that are displaced by the
    removal of other items in the view.

    For example, here is a view that specifies such a transition:

    \code
    ListView {
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
    \qmlproperty Transition QtQuick::ListView::displaced
    This property holds the generic transition to apply to items that have been displaced by
    any model operation that affects the view.

    This is a convenience for specifying the generic transition to be applied to any items
    that are displaced by an add, move or remove operation, without having to specify the
    individual addDisplaced, moveDisplaced and removeDisplaced properties. For example, here
    is a view that specifies a displaced transition:

    \code
    ListView {
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

void QQuickListView::viewportMoved(Qt::Orientations orient)
{
    Q_D(QQuickListView);
    QQuickItemView::viewportMoved(orient);

    if (!d->itemCount) {
        if (d->hasStickyHeader())
            d->updateHeader();
        if (d->hasStickyFooter())
            d->updateFooter();
        return;
    }

    // Recursion can occur due to refill changing the content size.
    if (d->inViewportMoved)
        return;
    d->inViewportMoved = true;

    if (yflick()) {
        if (d->isBottomToTop())
            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferAfter : QQuickListViewPrivate::BufferBefore;
        else
            d->bufferMode = d->vData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferBefore : QQuickListViewPrivate::BufferAfter;
    } else {
        if (d->isRightToLeft())
            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferAfter : QQuickListViewPrivate::BufferBefore;
        else
            d->bufferMode = d->hData.smoothVelocity < 0 ? QQuickListViewPrivate::BufferBefore : QQuickListViewPrivate::BufferAfter;
    }

    d->refillOrLayout();

    // Set visibility of items to eliminate cost of items outside the visible area.
    qreal from = d->isContentFlowReversed() ? -d->position()-d->displayMarginBeginning-d->size() : d->position()-d->displayMarginBeginning;
    qreal to = d->isContentFlowReversed() ? -d->position()+d->displayMarginEnd : d->position()+d->size()+d->displayMarginEnd;
    for (int i = 0; i < d->visibleItems.count(); ++i) {
        FxViewItem *item = static_cast<FxListItemSG*>(d->visibleItems.at(i));
        if (item->item)
            QQuickItemPrivate::get(item->item)->setCulled(item->endPosition() < from || item->position() > to);
    }
    if (d->currentItem)
        QQuickItemPrivate::get(d->currentItem->item)->setCulled(d->currentItem->endPosition() < from || d->currentItem->position() > to);

    if (d->hData.flicking || d->vData.flicking || d->hData.moving || d->vData.moving)
        d->moveReason = QQuickListViewPrivate::Mouse;
    if (d->moveReason != QQuickListViewPrivate::SetIndex) {
        if (d->haveHighlightRange && d->highlightRange == StrictlyEnforceRange && d->highlight) {
            // reposition highlight
            qreal pos = d->highlight->position();
            qreal viewPos = d->isContentFlowReversed() ? -d->position()-d->size() : d->position();
            if (pos > viewPos + d->highlightRangeEnd - d->highlight->size())
                pos = viewPos + d->highlightRangeEnd - d->highlight->size();
            if (pos < viewPos + d->highlightRangeStart)
                pos = viewPos + d->highlightRangeStart;
            if (pos != d->highlight->position()) {
                d->highlightPosAnimator->stop();
                static_cast<FxListItemSG*>(d->highlight)->setPosition(pos);
            } else {
                d->updateHighlight();
            }

            // update current index
            if (FxViewItem *snapItem = d->snapItemAt(d->highlight->position())) {
                if (snapItem->index >= 0 && snapItem->index != d->currentIndex)
                    d->updateCurrent(snapItem->index);
            }
        }
    }

    if ((d->hData.flicking || d->vData.flicking) && d->correctFlick && !d->inFlickCorrection) {
        d->inFlickCorrection = true;
        // Near an end and it seems that the extent has changed?
        // Recalculate the flick so that we don't end up in an odd position.
        if (yflick() && !d->vData.inOvershoot) {
            if (d->vData.velocity > 0) {
                const qreal minY = minYExtent();
                if ((minY - d->vData.move.value() < height()/2 || d->vData.flickTarget - d->vData.move.value() < height()/2)
                    && minY != d->vData.flickTarget)
                    d->flickY(-d->vData.smoothVelocity.value());
            } else if (d->vData.velocity < 0) {
                const qreal maxY = maxYExtent();
                if ((d->vData.move.value() - maxY < height()/2 || d->vData.move.value() - d->vData.flickTarget < height()/2)
                    && maxY != d->vData.flickTarget)
                    d->flickY(-d->vData.smoothVelocity.value());
            }
        }

        if (xflick() && !d->hData.inOvershoot) {
            if (d->hData.velocity > 0) {
                const qreal minX = minXExtent();
                if ((minX - d->hData.move.value() < width()/2 || d->hData.flickTarget - d->hData.move.value() < width()/2)
                    && minX != d->hData.flickTarget)
                    d->flickX(-d->hData.smoothVelocity.value());
            } else if (d->hData.velocity < 0) {
                const qreal maxX = maxXExtent();
                if ((d->hData.move.value() - maxX < width()/2 || d->hData.move.value() - d->hData.flickTarget < width()/2)
                    && maxX != d->hData.flickTarget)
                    d->flickX(-d->hData.smoothVelocity.value());
            }
        }
        d->inFlickCorrection = false;
    }
    if (d->hasStickyHeader())
        d->updateHeader();
    if (d->hasStickyFooter())
        d->updateFooter();
    if (d->sectionCriteria) {
        d->updateCurrentSection();
        d->updateStickySections();
    }
    d->inViewportMoved = false;
}

void QQuickListView::keyPressEvent(QKeyEvent *event)
{
    Q_D(QQuickListView);
    if (d->model && d->model->count() && ((d->interactive && !d->explicitKeyNavigationEnabled)
        || (d->explicitKeyNavigationEnabled && d->keyNavigationEnabled))) {
        if ((d->orient == QQuickListView::Horizontal && !d->isRightToLeft() && event->key() == Qt::Key_Left)
                    || (d->orient == QQuickListView::Horizontal && d->isRightToLeft() && event->key() == Qt::Key_Right)
                    || (d->orient == QQuickListView::Vertical && !d->isBottomToTop() && event->key() == Qt::Key_Up)
                    || (d->orient == QQuickListView::Vertical && d->isBottomToTop() && event->key() == Qt::Key_Down)) {
            if (currentIndex() > 0 || (d->wrap && !event->isAutoRepeat())) {
                decrementCurrentIndex();
                event->accept();
                return;
            } else if (d->wrap) {
                event->accept();
                return;
            }
        } else if ((d->orient == QQuickListView::Horizontal && !d->isRightToLeft() && event->key() == Qt::Key_Right)
                    || (d->orient == QQuickListView::Horizontal && d->isRightToLeft() && event->key() == Qt::Key_Left)
                   || (d->orient == QQuickListView::Vertical && !d->isBottomToTop() && event->key() == Qt::Key_Down)
                   || (d->orient == QQuickListView::Vertical && d->isBottomToTop() && event->key() == Qt::Key_Up)) {
            if (currentIndex() < d->model->count() - 1 || (d->wrap && !event->isAutoRepeat())) {
                incrementCurrentIndex();
                event->accept();
                return;
            } else if (d->wrap) {
                event->accept();
                return;
            }
        }
    }
    event->ignore();
    QQuickItemView::keyPressEvent(event);
}

void QQuickListView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickListView);
    if (d->isRightToLeft()) {
        // maintain position relative to the right edge
        qreal dx = newGeometry.width() - oldGeometry.width();
        setContentX(contentX() - dx);
    } else if (d->isBottomToTop()) {
        // maintain position relative to the bottom edge
        qreal dy = newGeometry.height() - oldGeometry.height();
        setContentY(contentY() - dy);
    }
    QQuickItemView::geometryChanged(newGeometry, oldGeometry);
}

void QQuickListView::initItem(int index, QObject *object)
{
    QQuickItemView::initItem(index, object);

    // setting the view from the FxViewItem wrapper is too late if the delegate
    // needs access to the view in Component.onCompleted
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        QQuickListViewAttached *attached = static_cast<QQuickListViewAttached *>(
                qmlAttachedPropertiesObject<QQuickListView>(item));
        if (attached)
            attached->setView(this);
    }
}


/*!
    \qmlmethod QtQuick::ListView::incrementCurrentIndex()

    Increments the current index.  The current index will wrap
    if keyNavigationWraps is true and it is currently at the end.
    This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickListView::incrementCurrentIndex()
{
    Q_D(QQuickListView);
    int count = d->model ? d->model->count() : 0;
    if (count && (currentIndex() < count - 1 || d->wrap)) {
        d->moveReason = QQuickListViewPrivate::SetIndex;
        int index = currentIndex()+1;
        setCurrentIndex((index >= 0 && index < count) ? index : 0);
    }
}

/*!
    \qmlmethod QtQuick::ListView::decrementCurrentIndex()

    Decrements the current index.  The current index will wrap
    if keyNavigationWraps is true and it is currently at the beginning.
    This method has no effect if the \l count is zero.

    \b Note: methods should only be called after the Component has completed.
*/
void QQuickListView::decrementCurrentIndex()
{
    Q_D(QQuickListView);
    int count = d->model ? d->model->count() : 0;
    if (count && (currentIndex() > 0 || d->wrap)) {
        d->moveReason = QQuickListViewPrivate::SetIndex;
        int index = currentIndex()-1;
        setCurrentIndex((index >= 0 && index < count) ? index : count-1);
    }
}

void QQuickListViewPrivate::updateSectionCriteria()
{
    Q_Q(QQuickListView);
    if (q->isComponentComplete() && model) {
        QList<QByteArray> roles;
        if (sectionCriteria && !sectionCriteria->property().isEmpty())
            roles << sectionCriteria->property().toUtf8();
        model->setWatchedRoles(roles);
        updateSections();
        if (itemCount)
            forceLayoutPolish();
    }
}

bool QQuickListViewPrivate::applyInsertionChange(const QQmlChangeSet::Change &change, ChangeResult *insertResult, QList<FxViewItem *> *addedItems, QList<MovedItem> *movingIntoView)
{
    int modelIndex = change.index;
    int count = change.count;

    qreal tempPos = isContentFlowReversed() ? -position()-size() : position();
    int index = visibleItems.count() ? mapFromModel(modelIndex) : 0;

    if (index < 0) {
        int i = visibleItems.count() - 1;
        while (i > 0 && visibleItems.at(i)->index == -1)
            --i;
        if (i == 0 && visibleItems.constFirst()->index == -1) {
            // there are no visible items except items marked for removal
            index = visibleItems.count();
        } else if (visibleItems.at(i)->index + 1 == modelIndex
            && visibleItems.at(i)->endPosition() <= buffer+displayMarginEnd+tempPos+size()) {
            // Special case of appending an item to the model.
            index = visibleItems.count();
        } else {
            if (modelIndex < visibleIndex) {
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

    // index can be the next item past the end of the visible items list (i.e. appended)
    qreal pos = 0;
    if (visibleItems.count()) {
        pos = index < visibleItems.count() ? visibleItems.at(index)->position()
                                                : visibleItems.constLast()->endPosition() + spacing;
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

    bool visibleAffected = false;
    if (insertResult->visiblePos.isValid() && pos < insertResult->visiblePos) {
        // Insert items before the visible item.
        int insertionIdx = index;
        int i = 0;
        qreal from = tempPos - displayMarginBeginning - buffer;

        if (insertionIdx < visibleIndex) {
            if (pos >= from) {
                // items won't be visible, just note the size for repositioning
                insertResult->sizeChangesBeforeVisiblePos += count * (averageSize + spacing);
            }
        } else {
            for (i = count-1; i >= 0 && pos >= from; --i) {
                // item is before first visible e.g. in cache buffer
                FxViewItem *item = 0;
                if (change.isMove() && (item = currentChanges.removedItems.take(change.moveKey(modelIndex + i))))
                    item->index = modelIndex + i;
                if (!item)
                    item = createItem(modelIndex + i);
                if (!item)
                    return false;

                visibleAffected = true;
                visibleItems.insert(insertionIdx, item);
                if (insertionIdx == 0)
                    insertResult->changedFirstItem = true;
                if (!change.isMove()) {
                    addedItems->append(item);
                    if (transitioner)
                        item->transitionNextReposition(transitioner, QQuickItemViewTransitioner::AddTransition, true);
                    else
                        static_cast<FxListItemSG *>(item)->setPosition(pos, true);
                }
                insertResult->sizeChangesBeforeVisiblePos += item->size() + spacing;
                pos -= item->size() + spacing;
                index++;
            }
        }

        int firstOkIdx = -1;
        for (int i = 0; i <= insertionIdx && i < visibleItems.count() - 1; i++) {
            if (visibleItems.at(i)->index + 1 != visibleItems.at(i + 1)->index) {
                firstOkIdx = i + 1;
                break;
            }
        }
        for (int i = 0; i < firstOkIdx; i++) {
            FxViewItem *nvItem = visibleItems.takeFirst();
            addedItems->removeOne(nvItem);
            removeItem(nvItem);
        }

    } else {
        qreal to = buffer + displayMarginEnd + tempPos + size();

        visibleAffected = count > 0 && pos < to;

        for (int i = 0; i < count && pos <= to; ++i) {
            FxViewItem *item = 0;
            if (change.isMove() && (item = currentChanges.removedItems.take(change.moveKey(modelIndex + i))))
                item->index = modelIndex + i;
            bool newItem = !item;
            if (!item)
                item = createItem(modelIndex + i);
            if (!item)
                return false;

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
                    static_cast<FxListItemSG *>(item)->setPosition(pos, true);
            }
            insertResult->sizeChangesAfterVisiblePos += item->size() + spacing;
            pos += item->size() + spacing;
            ++index;
        }

        if (0 < index && index < visibleItems.count()) {
            FxViewItem *prevItem = visibleItems.at(index - 1);
            FxViewItem *item = visibleItems.at(index);
            if (prevItem->index != item->index - 1) {
                int i = index;
                qreal prevPos = prevItem->position();
                while (i < visibleItems.count()) {
                    FxListItemSG *nvItem = static_cast<FxListItemSG *>(visibleItems.takeLast());
                    insertResult->sizeChangesAfterVisiblePos -= nvItem->size() + spacing;
                    addedItems->removeOne(nvItem);
                    if (nvItem->transitionScheduledOrRunning())
                        nvItem->setPosition(prevPos + (nvItem->index - prevItem->index) * averageSize);
                    removeItem(nvItem);
                }
            }
        }
    }

    updateVisibleIndex();

    return visibleAffected;
}

void QQuickListViewPrivate::translateAndTransitionItemsAfter(int afterModelIndex, const ChangeResult &insertionResult, const ChangeResult &removalResult)
{
    Q_UNUSED(insertionResult);

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
    qreal sizeRemoved = -removalResult.sizeChangesAfterVisiblePos
            - (removalResult.countChangeAfterVisibleItems * (averageSize + spacing));

    for (int i=markerItemIndex+1; i<visibleItems.count() && visibleItems.at(i)->position() < viewEndPos; i++) {
        FxListItemSG *listItem = static_cast<FxListItemSG *>(visibleItems.at(i));
        if (!listItem->transitionScheduledOrRunning()) {
            qreal pos = listItem->position();
            listItem->setPosition(pos - sizeRemoved);
            listItem->transitionNextReposition(transitioner, QQuickItemViewTransitioner::RemoveTransition, false);
            listItem->setPosition(pos);
        }
    }
}

/*!
    \qmlmethod QtQuick::ListView::positionViewAtIndex(int index, PositionMode mode)

    Positions the view such that the \a index is at the position specified by
    \a mode:

    \list
    \li ListView.Beginning - position item at the top (or left for horizontal orientation) of the view.
    \li ListView.Center - position item in the center of the view.
    \li ListView.End - position item at bottom (or right for horizontal orientation) of the view.
    \li ListView.Visible - if any part of the item is visible then take no action, otherwise
    bring the item into view.
    \li ListView.Contain - ensure the entire item is visible.  If the item is larger than
    the view the item is positioned at the top (or left for horizontal orientation) of the view.
    \li ListView.SnapPosition - position the item at \l preferredHighlightBegin.  This mode
    is only valid if \l highlightRangeMode is StrictlyEnforceRange or snapping is enabled
    via \l snapMode.
    \endlist

    If positioning the view at \a index would cause empty space to be displayed at
    the beginning or end of the view, the view will be positioned at the boundary.

    It is not recommended to use \l {Flickable::}{contentX} or \l {Flickable::}{contentY} to position the view
    at a particular index.  This is unreliable since removing items from the start
    of the list does not cause all other items to be repositioned, and because
    the actual start of the view can vary based on the size of the delegates.
    The correct way to bring an item into view is with \c positionViewAtIndex.

    \b Note: methods should only be called after the Component has completed.  To position
    the view at startup, this method should be called by Component.onCompleted.  For
    example, to position the view at the end:

    \code
    Component.onCompleted: positionViewAtIndex(count - 1, ListView.Beginning)
    \endcode
*/

/*!
    \qmlmethod QtQuick::ListView::positionViewAtBeginning()
    \qmlmethod QtQuick::ListView::positionViewAtEnd()

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
    \qmlmethod int QtQuick::ListView::indexAt(real x, real y)

    Returns the index of the visible item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, or the item is
    not visible -1 is returned.

    If the item is outside the visible area, -1 is returned, regardless of
    whether an item will exist at that point when scrolled into view.

    \b Note: methods should only be called after the Component has completed.
*/

/*!
    \qmlmethod Item QtQuick::ListView::itemAt(real x, real y)

    Returns the visible item containing the point \a x, \a y in content
    coordinates.  If there is no item at the point specified, or the item is
    not visible null is returned.

    If the item is outside the visible area, null is returned, regardless of
    whether an item will exist at that point when scrolled into view.

    \b Note: methods should only be called after the Component has completed.
*/

/*!
    \qmlmethod QtQuick::ListView::forceLayout()

    Responding to changes in the model is usually batched to happen only once
    per frame. This means that inside script blocks it is possible for the
    underlying model to have changed, but the ListView has not caught up yet.

    This method forces the ListView to immediately respond to any outstanding
    changes in the model.

    \since 5.1

    \b Note: methods should only be called after the Component has completed.
*/

QQuickListViewAttached *QQuickListView::qmlAttachedProperties(QObject *obj)
{
    return new QQuickListViewAttached(obj);
}

QT_END_NAMESPACE
