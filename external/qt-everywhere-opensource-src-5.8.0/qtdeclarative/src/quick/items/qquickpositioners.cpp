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

#include "qquickpositioners_p.h"
#include "qquickpositioners_p_p.h"

#include <QtQml/qqml.h>
#include <QtQml/qqmlinfo.h>
#include <QtCore/qcoreapplication.h>

#include <QtQuick/private/qquickstate_p.h>
#include <QtQuick/private/qquickstategroup_p.h>
#include <private/qquickstatechangescript_p.h>
#include <QtQuick/private/qquicktransition_p.h>

QT_BEGIN_NAMESPACE

static const QQuickItemPrivate::ChangeTypes watchedChanges
    = QQuickItemPrivate::Geometry
    | QQuickItemPrivate::SiblingOrder
    | QQuickItemPrivate::Visibility
    | QQuickItemPrivate::Destroyed;

void QQuickBasePositionerPrivate::watchChanges(QQuickItem *other)
{
    QQuickItemPrivate *otherPrivate = QQuickItemPrivate::get(other);
    otherPrivate->addItemChangeListener(this, watchedChanges);
}

void QQuickBasePositionerPrivate::unwatchChanges(QQuickItem* other)
{
    QQuickItemPrivate *otherPrivate = QQuickItemPrivate::get(other);
    otherPrivate->removeItemChangeListener(this, watchedChanges);
}


QQuickBasePositioner::PositionedItem::PositionedItem(QQuickItem *i)
    : item(i)
    , transitionableItem(0)
    , index(-1)
    , isNew(false)
    , isVisible(true)
    , topPadding(0)
    , leftPadding(0)
    , rightPadding(0)
    , bottomPadding(0)
{
}

QQuickBasePositioner::PositionedItem::~PositionedItem()
{
    delete transitionableItem;
}

qreal QQuickBasePositioner::PositionedItem::itemX() const
{
    return transitionableItem ? transitionableItem->itemX() : item->x();
}

qreal QQuickBasePositioner::PositionedItem::itemY() const
{
    return transitionableItem ? transitionableItem->itemY() : item->y();
}

void QQuickBasePositioner::PositionedItem::moveTo(const QPointF &pos)
{
    if (transitionableItem)
        transitionableItem->moveTo(pos);
    else
        item->setPosition(pos);
}

void QQuickBasePositioner::PositionedItem::transitionNextReposition(QQuickItemViewTransitioner *transitioner, QQuickItemViewTransitioner::TransitionType type, bool asTarget)
{
    if (!transitioner)
        return;
    if (!transitionableItem)
        transitionableItem = new QQuickItemViewTransitionableItem(item);
    transitioner->transitionNextReposition(transitionableItem, type, asTarget);
}

bool QQuickBasePositioner::PositionedItem::prepareTransition(QQuickItemViewTransitioner *transitioner, const QRectF &viewBounds)
{
    return transitionableItem ? transitionableItem->prepareTransition(transitioner, index, viewBounds) : false;
}

void QQuickBasePositioner::PositionedItem::startTransition(QQuickItemViewTransitioner *transitioner)
{
    if (transitionableItem)
        transitionableItem->startTransition(transitioner, index);
}

void QQuickBasePositioner::PositionedItem::updatePadding(qreal lp, qreal tp, qreal rp, qreal bp)
{
    leftPadding = lp;
    topPadding = tp;
    rightPadding = rp;
    bottomPadding = bp;
}

QQuickBasePositioner::QQuickBasePositioner(PositionerType at, QQuickItem *parent)
    : QQuickImplicitSizeItem(*(new QQuickBasePositionerPrivate), parent)
{
    Q_D(QQuickBasePositioner);
    d->init(at);
}
/*!
    \internal
    \class QQuickBasePositioner
    \brief For specifying a base for QQuickGraphics layouts

    To create a QQuickGraphics Positioner, simply subclass QQuickBasePositioner and implement
    doLayout(), which is automatically called when the layout might need
    updating. In doLayout() use the setX and setY functions from QQuickBasePositioner, and the
    base class will apply the positions along with the appropriate transitions. The items to
    position are provided in order as the protected member positionedItems.

    You also need to set a PositionerType, to declare whether you are positioning the x, y or both
    for the child items. Depending on the chosen type, only x or y changes will be applied.

    Note that the subclass is responsible for adding the spacing in between items.

    Positioning is batched and synchronized with painting to reduce the number of
    calculations needed. This means that positioners may not reposition items immediately
    when changes occur, but it will have moved by the next frame.
*/

QQuickBasePositioner::QQuickBasePositioner(QQuickBasePositionerPrivate &dd, PositionerType at, QQuickItem *parent)
    : QQuickImplicitSizeItem(dd, parent)
{
    Q_D(QQuickBasePositioner);
    d->init(at);
}

QQuickBasePositioner::~QQuickBasePositioner()
{
    Q_D(QQuickBasePositioner);
    delete d->transitioner;
    for (int i = 0; i < positionedItems.count(); ++i)
        d->unwatchChanges(positionedItems.at(i).item);
    for (int i = 0; i < unpositionedItems.count(); ++i)
        d->unwatchChanges(unpositionedItems.at(i).item);
    clearPositionedItems(&positionedItems);
    clearPositionedItems(&unpositionedItems);
}

void QQuickBasePositioner::updatePolish()
{
    Q_D(QQuickBasePositioner);
    if (d->positioningDirty)
        prePositioning();
}

qreal QQuickBasePositioner::spacing() const
{
    Q_D(const QQuickBasePositioner);
    return d->spacing;
}

void QQuickBasePositioner::setSpacing(qreal s)
{
    Q_D(QQuickBasePositioner);
    if (s == d->spacing)
        return;
    d->spacing = s;
    d->setPositioningDirty();
    emit spacingChanged();
}

QQuickTransition *QQuickBasePositioner::populate() const
{
    Q_D(const QQuickBasePositioner);
    return d->transitioner ? d->transitioner->populateTransition : 0;
}

void QQuickBasePositioner::setPopulate(QQuickTransition *transition)
{
    Q_D(QQuickBasePositioner);
    if (!d->transitioner)
        d->transitioner = new QQuickItemViewTransitioner;
    if (d->transitioner->populateTransition != transition) {
        d->transitioner->populateTransition = transition;
        emit populateChanged();
    }
}

QQuickTransition *QQuickBasePositioner::move() const
{
    Q_D(const QQuickBasePositioner);
    return d->transitioner ? d->transitioner->displacedTransition : 0;
}

void QQuickBasePositioner::setMove(QQuickTransition *mt)
{
    Q_D(QQuickBasePositioner);
    if (!d->transitioner)
        d->transitioner = new QQuickItemViewTransitioner;
    if (mt == d->transitioner->displacedTransition)
        return;

    d->transitioner->displacedTransition = mt;
    emit moveChanged();
}

QQuickTransition *QQuickBasePositioner::add() const
{
    Q_D(const QQuickBasePositioner);
    return d->transitioner ? d->transitioner->addTransition : 0;
}

void QQuickBasePositioner::setAdd(QQuickTransition *add)
{
    Q_D(QQuickBasePositioner);
    if (!d->transitioner)
        d->transitioner = new QQuickItemViewTransitioner;
    if (add == d->transitioner->addTransition)
        return;

    d->transitioner->addTransition = add;
    emit addChanged();
}

void QQuickBasePositioner::componentComplete()
{
    Q_D(QQuickBasePositioner);
    QQuickItem::componentComplete();
    if (d->transitioner)
        d->transitioner->setPopulateTransitionEnabled(true);
    positionedItems.reserve(childItems().count());
    prePositioning();
    if (d->transitioner)
        d->transitioner->setPopulateTransitionEnabled(false);
}

void QQuickBasePositioner::itemChange(ItemChange change, const ItemChangeData &value)
{
    Q_D(QQuickBasePositioner);
    if (change == ItemChildAddedChange) {
        d->setPositioningDirty();
    } else if (change == ItemChildRemovedChange) {
        QQuickItem *child = value.item;
        QQuickBasePositioner::PositionedItem posItem(child);
        int idx = positionedItems.find(posItem);
        if (idx >= 0) {
            d->unwatchChanges(child);
            removePositionedItem(&positionedItems, idx);
        } else if ((idx = unpositionedItems.find(posItem)) >= 0) {
            d->unwatchChanges(child);
            removePositionedItem(&unpositionedItems, idx);
        }
        d->setPositioningDirty();
    }

    QQuickItem::itemChange(change, value);
}

void QQuickBasePositioner::prePositioning()
{
    Q_D(QQuickBasePositioner);
    if (!isComponentComplete())
        return;

    if (d->doingPositioning)
        return;

    d->positioningDirty = false;
    d->doingPositioning = true;
    //Need to order children by creation order modified by stacking order
    QList<QQuickItem *> children = childItems();

    QPODVector<PositionedItem,8> oldItems;
    positionedItems.copyAndClear(oldItems);
    for (int ii = 0; ii < unpositionedItems.count(); ii++)
        oldItems.append(unpositionedItems[ii]);
    unpositionedItems.clear();
    int addedIndex = -1;

    for (int ii = 0; ii < children.count(); ++ii) {
        QQuickItem *child = children.at(ii);
        if (QQuickItemPrivate::get(child)->isTransparentForPositioner())
            continue;
        QQuickItemPrivate *childPrivate = QQuickItemPrivate::get(child);
        PositionedItem posItem(child);
        int wIdx = oldItems.find(posItem);
        if (wIdx < 0) {
            d->watchChanges(child);
            posItem.isNew = true;
            if (!childPrivate->explicitVisible || !child->width() || !child->height()) {
                posItem.isVisible = false;
                posItem.index = -1;
                unpositionedItems.append(posItem);
            } else {
                posItem.index = positionedItems.count();
                positionedItems.append(posItem);

                if (d->transitioner) {
                    if (addedIndex < 0)
                        addedIndex = posItem.index;
                    PositionedItem *theItem = &positionedItems[positionedItems.count()-1];
                    if (d->transitioner->canTransition(QQuickItemViewTransitioner::PopulateTransition, true))
                        theItem->transitionNextReposition(d->transitioner, QQuickItemViewTransitioner::PopulateTransition, true);
                    else if (!d->transitioner->populateTransitionEnabled())
                        theItem->transitionNextReposition(d->transitioner, QQuickItemViewTransitioner::AddTransition, true);
                }
            }
        } else {
            PositionedItem *item = &oldItems[wIdx];
            // Items are only omitted from positioning if they are explicitly hidden
            // i.e. their positioning is not affected if an ancestor is hidden.
            if (!childPrivate->explicitVisible || !child->width() || !child->height()) {
                item->isVisible = false;
                item->index = -1;
                unpositionedItems.append(*item);
            } else if (!item->isVisible) {
                // item changed from non-visible to visible, treat it as a "new" item
                item->isVisible = true;
                item->isNew = true;
                item->index = positionedItems.count();
                positionedItems.append(*item);

                if (d->transitioner) {
                    if (addedIndex < 0)
                        addedIndex = item->index;
                    positionedItems[positionedItems.count()-1].transitionNextReposition(d->transitioner, QQuickItemViewTransitioner::AddTransition, true);
                }
            } else {
                item->isNew = false;
                item->index = positionedItems.count();
                positionedItems.append(*item);
            }
        }
    }

    if (d->transitioner) {
        for (int i=0; i<positionedItems.count(); i++) {
            if (!positionedItems[i].isNew) {
                if (addedIndex >= 0) {
                    positionedItems[i].transitionNextReposition(d->transitioner, QQuickItemViewTransitioner::AddTransition, false);
                } else {
                    // just queue the item for a move-type displace - if the item hasn't
                    // moved anywhere, it won't be transitioned anyway
                    positionedItems[i].transitionNextReposition(d->transitioner, QQuickItemViewTransitioner::MoveTransition, false);
                }
            }
        }
    }

    QSizeF contentSize(0,0);
    reportConflictingAnchors();
    if (!d->anchorConflict) {
        doPositioning(&contentSize);
        updateAttachedProperties();
    }

    if (d->transitioner) {
        QRectF viewBounds(QPointF(), contentSize);
        for (int i=0; i<positionedItems.count(); i++)
            positionedItems[i].prepareTransition(d->transitioner, viewBounds);
        for (int i=0; i<positionedItems.count(); i++)
            positionedItems[i].startTransition(d->transitioner);
        d->transitioner->resetTargetLists();
    }

    d->doingPositioning = false;

    //Set implicit size to the size of its children
    setImplicitSize(contentSize.width(), contentSize.height());
}

void QQuickBasePositioner::positionItem(qreal x, qreal y, PositionedItem *target)
{
    if ( target->itemX() != x || target->itemY() != y )
        target->moveTo(QPointF(x, y));
}

void QQuickBasePositioner::positionItemX(qreal x, PositionedItem *target)
{
    Q_D(QQuickBasePositioner);
    if (target->itemX() != x
            && (d->type == Horizontal || d->type == Both)) {
        target->moveTo(QPointF(x, target->itemY()));
    }
}

void QQuickBasePositioner::positionItemY(qreal y, PositionedItem *target)
{
    Q_D(QQuickBasePositioner);
    if (target->itemY() != y
            && (d->type == Vertical || d->type == Both)) {
        target->moveTo(QPointF(target->itemX(), y));
    }
}

/*
  Since PositionedItem values are stored by value, their internal transitionableItem pointers
  must be cleaned up when a PositionedItem is removed from a QPODVector, otherwise the pointer
  is never deleted since QPODVector doesn't invoke the destructor.
  */
void QQuickBasePositioner::removePositionedItem(QPODVector<PositionedItem,8> *items, int index)
{
    Q_ASSERT(index >= 0 && index < items->count());
    delete items->at(index).transitionableItem;
    items->remove(index);
}
void QQuickBasePositioner::clearPositionedItems(QPODVector<PositionedItem,8> *items)
{
    for (int i=0; i<items->count(); i++)
        delete items->at(i).transitionableItem;
    items->clear();
}

QQuickPositionerAttached *QQuickBasePositioner::qmlAttachedProperties(QObject *obj)
{
    return new QQuickPositionerAttached(obj);
}

void QQuickBasePositioner::updateAttachedProperties(QQuickPositionerAttached *specificProperty, QQuickItem *specificPropertyOwner) const
{
    // If this function is deemed too expensive or shows up in profiles, it could
    // be changed to run only when there are attached properties present. This
    // could be a flag in the positioner that is set by the attached property
    // constructor.
    QQuickPositionerAttached *prevLastProperty = 0;
    QQuickPositionerAttached *lastProperty = 0;

    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        const PositionedItem &child = positionedItems.at(ii);
        if (!child.item)
            continue;

        QQuickPositionerAttached *property = 0;

        if (specificProperty) {
            if (specificPropertyOwner == child.item) {
                property = specificProperty;
            }
        } else {
            property = static_cast<QQuickPositionerAttached *>(qmlAttachedPropertiesObject<QQuickBasePositioner>(child.item, false));
        }

        if (property) {
            property->setIndex(ii);
            property->setIsFirstItem(ii == 0);

            if (property->isLastItem()) {
                if (prevLastProperty)
                    prevLastProperty->setIsLastItem(false); // there can be only one last property
                prevLastProperty = property;
            }
        }

        lastProperty = property;
    }

    if (prevLastProperty && prevLastProperty != lastProperty)
        prevLastProperty->setIsLastItem(false);
    if (lastProperty)
        lastProperty->setIsLastItem(true);

    // clear attached properties for unpositioned items
    for (int ii = 0; ii < unpositionedItems.count(); ++ii) {
        const PositionedItem &child = unpositionedItems.at(ii);
        if (!child.item)
            continue;

        QQuickPositionerAttached *property = 0;

        if (specificProperty) {
            if (specificPropertyOwner == child.item) {
                property = specificProperty;
            }
        } else {
            property = static_cast<QQuickPositionerAttached *>(qmlAttachedPropertiesObject<QQuickBasePositioner>(child.item, false));
        }

        if (property) {
            property->setIndex(-1);
            property->setIsFirstItem(false);
            property->setIsLastItem(false);
        }
    }
}

qreal QQuickBasePositioner::padding() const
{
    Q_D(const QQuickBasePositioner);
    return d->padding();
}

void QQuickBasePositioner::setPadding(qreal padding)
{
    Q_D(QQuickBasePositioner);
    if (qFuzzyCompare(d->padding(), padding))
        return;

    d->extra.value().padding = padding;
    d->setPositioningDirty();
    emit paddingChanged();
    if (!d->extra.isAllocated() || !d->extra->explicitTopPadding)
        emit topPaddingChanged();
    if (!d->extra.isAllocated() || !d->extra->explicitLeftPadding)
        emit leftPaddingChanged();
    if (!d->extra.isAllocated() || !d->extra->explicitRightPadding)
        emit rightPaddingChanged();
    if (!d->extra.isAllocated() || !d->extra->explicitBottomPadding)
        emit bottomPaddingChanged();
}

void QQuickBasePositioner::resetPadding()
{
    setPadding(0);
}

qreal QQuickBasePositioner::topPadding() const
{
    Q_D(const QQuickBasePositioner);
    if (d->extra.isAllocated() && d->extra->explicitTopPadding)
        return d->extra->topPadding;
    return d->padding();
}

void QQuickBasePositioner::setTopPadding(qreal padding)
{
    Q_D(QQuickBasePositioner);
    d->setTopPadding(padding);
}

void QQuickBasePositioner::resetTopPadding()
{
    Q_D(QQuickBasePositioner);
    d->setTopPadding(0, true);
}

qreal QQuickBasePositioner::leftPadding() const
{
    Q_D(const QQuickBasePositioner);
    if (d->extra.isAllocated() && d->extra->explicitLeftPadding)
        return d->extra->leftPadding;
    return d->padding();
}

void QQuickBasePositioner::setLeftPadding(qreal padding)
{
    Q_D(QQuickBasePositioner);
    d->setLeftPadding(padding);
}

void QQuickBasePositioner::resetLeftPadding()
{
    Q_D(QQuickBasePositioner);
    d->setLeftPadding(0, true);
}

qreal QQuickBasePositioner::rightPadding() const
{
    Q_D(const QQuickBasePositioner);
    if (d->extra.isAllocated() && d->extra->explicitRightPadding)
        return d->extra->rightPadding;
    return d->padding();
}

void QQuickBasePositioner::setRightPadding(qreal padding)
{
    Q_D(QQuickBasePositioner);
    d->setRightPadding(padding);
}

void QQuickBasePositioner::resetRightPadding()
{
    Q_D(QQuickBasePositioner);
    d->setRightPadding(0, true);
}

qreal QQuickBasePositioner::bottomPadding() const
{
    Q_D(const QQuickBasePositioner);
    if (d->extra.isAllocated() && d->extra->explicitBottomPadding)
        return d->extra->bottomPadding;
    return d->padding();
}

void QQuickBasePositioner::setBottomPadding(qreal padding)
{
    Q_D(QQuickBasePositioner);
    d->setBottomPadding(padding);
}

void QQuickBasePositioner::resetBottomPadding()
{
    Q_D(QQuickBasePositioner);
    d->setBottomPadding(0, true);
}

QQuickBasePositionerPrivate::ExtraData::ExtraData()
    : padding(0)
    , topPadding(0)
    , leftPadding(0)
    , rightPadding(0)
    , bottomPadding(0)
    , explicitTopPadding(false)
    , explicitLeftPadding(false)
    , explicitRightPadding(false)
    , explicitBottomPadding(false)
{
}

void QQuickBasePositionerPrivate::setTopPadding(qreal value, bool reset)
{
    Q_Q(QQuickBasePositioner);
    qreal oldPadding = q->topPadding();
    if (!reset || extra.isAllocated()) {
        extra.value().topPadding = value;
        extra.value().explicitTopPadding = !reset;
    }
    if ((!reset && !qFuzzyCompare(oldPadding, value)) || (reset && !qFuzzyCompare(oldPadding, padding()))) {
        setPositioningDirty();
        emit q->topPaddingChanged();
    }
}

void QQuickBasePositionerPrivate::setLeftPadding(qreal value, bool reset)
{
    Q_Q(QQuickBasePositioner);
    qreal oldPadding = q->leftPadding();
    if (!reset || extra.isAllocated()) {
        extra.value().leftPadding = value;
        extra.value().explicitLeftPadding = !reset;
    }
    if ((!reset && !qFuzzyCompare(oldPadding, value)) || (reset && !qFuzzyCompare(oldPadding, padding()))) {
        setPositioningDirty();
        emit q->leftPaddingChanged();
    }
}

void QQuickBasePositionerPrivate::setRightPadding(qreal value, bool reset)
{
    Q_Q(QQuickBasePositioner);
    qreal oldPadding = q->rightPadding();
    if (!reset || extra.isAllocated()) {
        extra.value().rightPadding = value;
        extra.value().explicitRightPadding = !reset;
    }
    if ((!reset && !qFuzzyCompare(oldPadding, value)) || (reset && !qFuzzyCompare(oldPadding, padding()))) {
        setPositioningDirty();
        emit q->rightPaddingChanged();
    }
}

void QQuickBasePositionerPrivate::setBottomPadding(qreal value, bool reset)
{
    Q_Q(QQuickBasePositioner);
    qreal oldPadding = q->bottomPadding();
    if (!reset || extra.isAllocated()) {
        extra.value().bottomPadding = value;
        extra.value().explicitBottomPadding = !reset;
    }
    if ((!reset && !qFuzzyCompare(oldPadding, value)) || (reset && !qFuzzyCompare(oldPadding, padding()))) {
        setPositioningDirty();
        emit q->bottomPaddingChanged();
    }
}

/*!
    \qmltype Positioner
    \instantiates QQuickPositionerAttached
    \inqmlmodule QtQuick
    \ingroup qtquick-positioners
    \brief Provides attached properties that contain details on where an item exists in a positioner

    An object of type Positioner is attached to the top-level child item within a
    Column, Row, Flow or Grid. It provides properties that allow a child item to determine
    where it exists within the layout of its parent Column, Row, Flow or Grid.

    For example, below is a \l Grid with 16 child rectangles, as created through a \l Repeater.
    Each \l Rectangle displays its index in the Grid using \l {Positioner::index}{Positioner.index}, and the first
    item is colored differently by taking \l {Positioner::isFirstItem}{Positioner.isFirstItem} into account:

    \code
    Grid {
        Repeater {
            model: 16

            Rectangle {
                id: rect
                width: 30; height: 30
                border.width: 1
                color: Positioner.isFirstItem ? "yellow" : "lightsteelblue"

                Text { text: rect.Positioner.index }
            }
        }
    }
    \endcode

    \image positioner-example.png
*/

QQuickPositionerAttached::QQuickPositionerAttached(QObject *parent) : QObject(parent), m_index(-1), m_isFirstItem(false), m_isLastItem(false)
{
    QQuickItem *attachedItem = qobject_cast<QQuickItem *>(parent);
    if (attachedItem) {
        QQuickBasePositioner *positioner = qobject_cast<QQuickBasePositioner *>(attachedItem->parent());
        if (positioner) {
            positioner->updateAttachedProperties(this, attachedItem);
        }
    }
}

/*!
    \qmlattachedproperty int QtQuick::Positioner::index

    This property allows the item to determine
    its index within the positioner.
*/
void QQuickPositionerAttached::setIndex(int index)
{
    if (m_index == index)
        return;
    m_index = index;
    emit indexChanged();
}

/*!
    \qmlattachedproperty bool QtQuick::Positioner::isFirstItem
    \qmlattachedproperty bool QtQuick::Positioner::isLastItem

    These properties allow the item to determine if it
    is the first or last item in the positioner, respectively.
*/
void QQuickPositionerAttached::setIsFirstItem(bool isFirstItem)
{
    if (m_isFirstItem == isFirstItem)
        return;
    m_isFirstItem = isFirstItem;
    emit isFirstItemChanged();
}

void QQuickPositionerAttached::setIsLastItem(bool isLastItem)
{
    if (m_isLastItem == isLastItem)
        return;
    m_isLastItem = isLastItem;
    emit isLastItemChanged();
}

/*!
    \qmltype Column
    \instantiates QQuickColumn
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-positioners
    \brief Positions its children in a column

    Column is a type that positions its child items along a single column.
    It can be used as a convenient way to vertically position a series of items without
    using \l {Positioning with Anchors}{anchors}.

    Below is a Column that contains three rectangles of various sizes:

    \snippet qml/column/vertical-positioner.qml document

    The Column automatically positions these items in a vertical formation, like this:

    \image verticalpositioner_example.png

    If an item within a Column is not \l {Item::}{visible}, or if it has a width or
    height of 0, the item will not be laid out and it will not be visible within the
    column. Also, since a Column automatically positions its children vertically, a child
    item within a Column should not set its \l {Item::y}{y} position or vertically
    anchor itself using the \l {Item::anchors.top}{top}, \l {Item::anchors.bottom}{bottom},
    \l {Item::anchors.verticalCenter}{anchors.verticalCenter}, \l {Item::anchors.fill}{fill}
    or \l {Item::anchors.centerIn}{centerIn} anchors. If you need to perform these actions,
    consider positioning the items without the use of a Column.

    Note that items in a Column can use the \l Positioner attached property to access
    more information about its position within the Column.

    For more information on using Column and other related positioner-types, see
    \l{Item Positioners}.


    \section1 Using Transitions

    A Column animate items using specific transitions when items are added to or moved
    within a Column.

    For example, the Column below sets the \l move property to a specific \l Transition:

    \snippet qml/column/column-transitions.qml document

    When the Space key is pressed, the \l {Item::visible}{visible} value of the green
    \l Rectangle is toggled. As it appears and disappears, the blue \l Rectangle moves within
    the Column, and the \l move transition is automatically applied to the blue \l Rectangle:

    \image verticalpositioner_transition.gif

    \sa Row, Grid, Flow, Positioner, ColumnLayout, {Qt Quick Examples - Positioners}
*/
/*!
    \since 5.6
    \qmlproperty real QtQuick::Column::padding
    \qmlproperty real QtQuick::Column::topPadding
    \qmlproperty real QtQuick::Column::leftPadding
    \qmlproperty real QtQuick::Column::bottomPadding
    \qmlproperty real QtQuick::Column::rightPadding

    These properties hold the padding around the content.
*/
/*!
    \qmlproperty Transition QtQuick::Column::populate

    This property holds the transition to be run for items that are part of
    this positioner at the time of its creation. The transition is run when the positioner
    is first created.

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Column::add

    This property holds the transition to be run for items that are added to this
    positioner. For a positioner, this applies to:

    \list
    \li Items that are created or reparented as a child of the positioner after the
        positioner has been created
    \li Child items that change their \l Item::visible property from false to true, and thus
       are now visible
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note This transition is not applied to the items that are already part of the positioner
    at the time of its creation. In this case, the \l populate transition is applied instead.

    \sa populate, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Column::move

    This property holds the transition to run for items that have moved within the
    positioner. For a positioner, this applies to:

    \list
    \li Child items that move when they are displaced due to the addition, removal or
       rearrangement of other items in the positioner
    \li Child items that are repositioned due to the resizing of other items in the positioner
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being moved. Note, however, that for this move transition, the
    ViewTransition.targetIndexes and ViewTransition.targetItems lists are only set when
    this transition is triggered by the addition of other items in the positioner; in other
    cases, these lists will be empty.  See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note In \l {Qt Quick 1}, this transition was applied to all items that were part of the
    positioner at the time of its creation. From \l {Qt Quick}{Qt Quick 2} onwards, positioners apply the
    \l populate transition to these items instead.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
  \qmlproperty real QtQuick::Column::spacing

  The spacing is the amount in pixels left empty between adjacent
  items. The default spacing is 0.

  \sa Grid::spacing
*/
QQuickColumn::QQuickColumn(QQuickItem *parent)
: QQuickBasePositioner(Vertical, parent)
{
}

void QQuickColumn::doPositioning(QSizeF *contentSize)
{
    //Precondition: All items in the positioned list have a valid item pointer and should be positioned
    qreal voffset = topPadding();
    const qreal padding = leftPadding() + rightPadding();
    contentSize->setWidth(qMax(contentSize->width(), padding));

    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        PositionedItem &child = positionedItems[ii];
        positionItem(child.itemX() + leftPadding() - child.leftPadding, voffset, &child);
        child.updatePadding(leftPadding(), topPadding(), rightPadding(), bottomPadding());
        contentSize->setWidth(qMax(contentSize->width(), child.item->width() + padding));

        voffset += child.item->height();
        voffset += spacing();
    }

    if (voffset - topPadding() != 0)//If we positioned any items, undo the spacing from the last item
        voffset -= spacing();
    contentSize->setHeight(voffset + bottomPadding());
}

void QQuickColumn::reportConflictingAnchors()
{
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate*>(QQuickBasePositionerPrivate::get(this));
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        const PositionedItem &child = positionedItems.at(ii);
        if (child.item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(static_cast<QQuickItem *>(child.item))->_anchors;
            if (anchors) {
                QQuickAnchors::Anchors usedAnchors = anchors->usedAnchors();
                if (usedAnchors & QQuickAnchors::TopAnchor ||
                    usedAnchors & QQuickAnchors::BottomAnchor ||
                    usedAnchors & QQuickAnchors::VCenterAnchor ||
                    anchors->fill() || anchors->centerIn()) {
                    d->anchorConflict = true;
                    break;
                }
            }
        }
    }
    if (d->anchorConflict) {
        qmlInfo(this) << "Cannot specify top, bottom, verticalCenter, fill or centerIn anchors for items inside Column."
            << " Column will not function.";
    }
}
/*!
    \qmltype Row
    \instantiates QQuickRow
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-positioners
    \brief Positions its children in a row

    Row is a type that positions its child items along a single row.
    It can be used as a convenient way to horizontally position a series of items without
    using \l {Positioning with Anchors}{anchors}.

    Below is a Row that contains three rectangles of various sizes:

    \snippet qml/row/row.qml document

    The Row automatically positions these items in a horizontal formation, like this:

    \image horizontalpositioner_example.png

    If an item within a Row is not \l {Item::}{visible}, or if it has a width or
    height of 0, the item will not be laid out and it will not be visible within the
    row. Also, since a Row automatically positions its children horizontally, a child
    item within a Row should not set its \l {Item::x}{x} position or horizontally
    anchor itself using the \l {Item::anchors.left}{left}, \l {Item::anchors.right}{right},
    \l {Item::anchors.horizontalCenter}{anchors.horizontalCenter}, \l {Item::anchors.fill}{fill}
    or \l {Item::anchors.centerIn}{centerIn} anchors. If you need to perform these actions,
    consider positioning the items without the use of a Row.

    Note that items in a Row can use the \l Positioner attached property to access
    more information about its position within the Row.

    For more information on using Row and other related positioner-types, see
    \l{Item Positioners}.


    \sa Column, Grid, Flow, Positioner, RowLayout, {Qt Quick Examples - Positioners}
*/
/*!
    \since 5.6
    \qmlproperty real QtQuick::Row::padding
    \qmlproperty real QtQuick::Row::topPadding
    \qmlproperty real QtQuick::Row::leftPadding
    \qmlproperty real QtQuick::Row::bottomPadding
    \qmlproperty real QtQuick::Row::rightPadding

    These properties hold the padding around the content.
*/
/*!
    \qmlproperty Transition QtQuick::Row::populate

    This property holds the transition to be run for items that are part of
    this positioner at the time of its creation. The transition is run when the positioner
    is first created.

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Row::add

    This property holds the transition to be run for items that are added to this
    positioner. For a positioner, this applies to:

    \list
    \li Items that are created or reparented as a child of the positioner after the
        positioner has been created
    \li Child items that change their \l Item::visible property from false to true, and thus
       are now visible
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note This transition is not applied to the items that are already part of the positioner
    at the time of its creation. In this case, the \l populate transition is applied instead.

    \sa populate, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Row::move

    This property holds the transition to run for items that have moved within the
    positioner. For a positioner, this applies to:

    \list
    \li Child items that move when they are displaced due to the addition, removal or
       rearrangement of other items in the positioner
    \li Child items that are repositioned due to the resizing of other items in the positioner
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being moved. Note, however, that for this move transition, the
    ViewTransition.targetIndexes and ViewTransition.targetItems lists are only set when
    this transition is triggered by the addition of other items in the positioner; in other
    cases, these lists will be empty.  See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note In \l {Qt Quick 1}, this transition was applied to all items that were part of the
    positioner at the time of its creation. From \l {Qt Quick}{QtQuick 2} onwards, positioners apply the
    \l populate transition to these items instead.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
  \qmlproperty real QtQuick::Row::spacing

  The spacing is the amount in pixels left empty between adjacent
  items. The default spacing is 0.

  \sa Grid::spacing
*/

class QQuickRowPrivate : public QQuickBasePositionerPrivate
{
    Q_DECLARE_PUBLIC(QQuickRow)

public:
    QQuickRowPrivate()
        : QQuickBasePositionerPrivate()
    {}

    void effectiveLayoutDirectionChange()
    {
        Q_Q(QQuickRow);
        // For RTL layout the positioning changes when the width changes.
        if (getEffectiveLayoutDirection(q) == Qt::RightToLeft)
            addItemChangeListener(this, QQuickItemPrivate::Geometry);
        else
            removeItemChangeListener(this, QQuickItemPrivate::Geometry);
        // Don't postpone, as it might be the only trigger for visible changes.
        q->prePositioning();
        emit q->effectiveLayoutDirectionChanged();
    }
};

QQuickRow::QQuickRow(QQuickItem *parent)
: QQuickBasePositioner(*new QQuickRowPrivate, Horizontal, parent)
{
}
/*!
    \qmlproperty enumeration QtQuick::Row::layoutDirection

    This property holds the layoutDirection of the row.

    Possible values:

    \list
    \li Qt.LeftToRight (default) - Items are laid out from left to right. If the width of the row is explicitly set,
    the left anchor remains to the left of the row.
    \li Qt.RightToLeft - Items are laid out from right to left. If the width of the row is explicitly set,
    the right anchor remains to the right of the row.
    \endlist

    \sa Grid::layoutDirection, Flow::layoutDirection, {Qt Quick Examples - Right to Left}
*/

Qt::LayoutDirection QQuickRow::layoutDirection() const
{
    return QQuickBasePositionerPrivate::getLayoutDirection(this);
}

void QQuickRow::setLayoutDirection(Qt::LayoutDirection layoutDirection)
{
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate* >(QQuickBasePositionerPrivate::get(this));
    if (d->layoutDirection != layoutDirection) {
        d->layoutDirection = layoutDirection;
        emit layoutDirectionChanged();
        d->effectiveLayoutDirectionChange();
    }
}
/*!
    \qmlproperty enumeration QtQuick::Row::effectiveLayoutDirection
    This property holds the effective layout direction of the row.

    When using the attached property \l {LayoutMirroring::enabled}{LayoutMirroring::enabled} for locale layouts,
    the visual layout direction of the row positioner will be mirrored. However, the
    property \l {Row::layoutDirection}{layoutDirection} will remain unchanged.

    \sa Row::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/

Qt::LayoutDirection QQuickRow::effectiveLayoutDirection() const
{
    return QQuickBasePositionerPrivate::getEffectiveLayoutDirection(this);
}

void QQuickRow::doPositioning(QSizeF *contentSize)
{
    //Precondition: All items in the positioned list have a valid item pointer and should be positioned
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate* >(QQuickBasePositionerPrivate::get(this));
    qreal hoffset1 = leftPadding();
    qreal hoffset2 = rightPadding();
    if (!d->isLeftToRight())
        qSwap(hoffset1, hoffset2);
    qreal hoffset = hoffset1;
    const qreal padding = topPadding() + bottomPadding();
    contentSize->setHeight(qMax(contentSize->height(), padding));

    QList<qreal> hoffsets;
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        PositionedItem &child = positionedItems[ii];

        if (d->isLeftToRight()) {
            positionItem(hoffset, child.itemY() + topPadding() - child.topPadding, &child);
            child.updatePadding(leftPadding(), topPadding(), rightPadding(), bottomPadding());
        } else {
            hoffsets << hoffset;
        }

        contentSize->setHeight(qMax(contentSize->height(), child.item->height() + padding));

        hoffset += child.item->width();
        hoffset += spacing();
    }

    if (hoffset - hoffset1 != 0)//If we positioned any items, undo the extra spacing from the last item
        hoffset -= spacing();
    contentSize->setWidth(hoffset + hoffset2);

    if (d->isLeftToRight())
        return;

    //Right to Left layout
    qreal end = 0;
    if (!widthValid())
        end = contentSize->width();
    else
        end = width();

    int acc = 0;
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        PositionedItem &child = positionedItems[ii];
        hoffset = end - hoffsets[acc++] - child.item->width();
        positionItem(hoffset, child.itemY() + topPadding() - child.topPadding, &child);
        child.updatePadding(leftPadding(), topPadding(), rightPadding(), bottomPadding());
    }
}

void QQuickRow::reportConflictingAnchors()
{
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate*>(QQuickBasePositionerPrivate::get(this));
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        const PositionedItem &child = positionedItems.at(ii);
        if (child.item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(static_cast<QQuickItem *>(child.item))->_anchors;
            if (anchors) {
                QQuickAnchors::Anchors usedAnchors = anchors->usedAnchors();
                if (usedAnchors & QQuickAnchors::LeftAnchor ||
                    usedAnchors & QQuickAnchors::RightAnchor ||
                    usedAnchors & QQuickAnchors::HCenterAnchor ||
                    anchors->fill() || anchors->centerIn()) {
                    d->anchorConflict = true;
                    break;
                }
            }
        }
    }
    if (d->anchorConflict)
        qmlInfo(this) << "Cannot specify left, right, horizontalCenter, fill or centerIn anchors for items inside Row."
            << " Row will not function.";
}

/*!
    \qmltype Grid
    \instantiates QQuickGrid
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-positioners
    \brief Positions its children in grid formation

    Grid is a type that positions its child items in grid formation.

    A Grid creates a grid of cells that is large enough to hold all of its
    child items, and places these items in the cells from left to right
    and top to bottom. Each item is positioned at the top-left corner of its
    cell with position (0, 0).

    A Grid defaults to four columns, and creates as many rows as are necessary to
    fit all of its child items. The number of rows and columns can be constrained
    by setting the \l rows and \l columns properties.

    For example, below is a Grid that contains five rectangles of various sizes:

    \snippet qml/grid/grid.qml document

    The Grid automatically positions the child items in a grid formation:

    \image gridLayout_example.png

    If an item within a Grid is not \l {Item::}{visible}, or if it has a width or
    height of 0, the item will not be laid out and it will not be visible within the
    column. Also, since a Grid automatically positions its children, a child
    item within a Grid should not set its \l {Item::x}{x} or \l {Item::y}{y} positions
    or anchor itself with any of the \l {Item::anchors}{anchor} properties.

    For more information on using Grid and other related positioner-types, see
    \l{Item Positioners}.


    \sa Flow, Row, Column, Positioner, GridLayout, {Qt Quick Examples - Positioners}
*/
/*!
    \since 5.6
    \qmlproperty real QtQuick::Grid::padding
    \qmlproperty real QtQuick::Grid::topPadding
    \qmlproperty real QtQuick::Grid::leftPadding
    \qmlproperty real QtQuick::Grid::bottomPadding
    \qmlproperty real QtQuick::Grid::rightPadding

    These properties hold the padding around the content.
*/
/*!
    \qmlproperty Transition QtQuick::Grid::populate

    This property holds the transition to be run for items that are part of
    this positioner at the time of its creation. The transition is run when the positioner
    is first created.

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Grid::add

    This property holds the transition to be run for items that are added to this
    positioner. For a positioner, this applies to:

    \list
    \li Items that are created or reparented as a child of the positioner after the
        positioner has been created
    \li Child items that change their \l Item::visible property from false to true, and thus
       are now visible
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note This transition is not applied to the items that are already part of the positioner
    at the time of its creation. In this case, the \l populate transition is applied instead.

    \sa populate, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Grid::move

    This property holds the transition to run for items that have moved within the
    positioner. For a positioner, this applies to:

    \list
    \li Child items that move when they are displaced due to the addition, removal or
       rearrangement of other items in the positioner
    \li Child items that are repositioned due to the resizing of other items in the positioner
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being moved. Note, however, that for this move transition, the
    ViewTransition.targetIndexes and ViewTransition.targetItems lists are only set when
    this transition is triggered by the addition of other items in the positioner; in other
    cases, these lists will be empty.  See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note In \l {Qt Quick 1}, this transition was applied to all items that were part of the
    positioner at the time of its creation. From \l {Qt Quick}{QtQuick 2} onwards, positioners apply the
    \l populate transition to these items instead.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
  \qmlproperty qreal QtQuick::Grid::spacing

  The spacing is the amount in pixels left empty between adjacent
  items. The amount of spacing applied will be the same in the
  horizontal and vertical directions. The default spacing is 0.

  The below example places a Grid containing a red, a blue and a
  green rectangle on a gray background. The area the grid positioner
  occupies is colored white. The positioner on the left has the
  no spacing (the default), and the positioner on the right has
  a spacing of 6.

  \inlineimage qml-grid-no-spacing.png
  \inlineimage qml-grid-spacing.png

  \sa rows, columns
*/

class QQuickGridPrivate : public QQuickBasePositionerPrivate
{
    Q_DECLARE_PUBLIC(QQuickGrid)

public:
    QQuickGridPrivate()
        : QQuickBasePositionerPrivate()
    {}

    void effectiveLayoutDirectionChange()
    {
        Q_Q(QQuickGrid);
        // For RTL layout the positioning changes when the width changes.
        if (getEffectiveLayoutDirection(q) == Qt::RightToLeft)
            addItemChangeListener(this, QQuickItemPrivate::Geometry);
        else
            removeItemChangeListener(this, QQuickItemPrivate::Geometry);
        // Don't postpone, as it might be the only trigger for visible changes.
        q->prePositioning();
        emit q->effectiveLayoutDirectionChanged();
        emit q->effectiveHorizontalAlignmentChanged(q->effectiveHAlign());
    }
};

QQuickGrid::QQuickGrid(QQuickItem *parent)
    : QQuickBasePositioner(*new QQuickGridPrivate, Both, parent)
    , m_rows(-1)
    , m_columns(-1)
    , m_rowSpacing(-1)
    , m_columnSpacing(-1)
    , m_useRowSpacing(false)
    , m_useColumnSpacing(false)
    , m_flow(LeftToRight)
    , m_hItemAlign(AlignLeft)
    , m_vItemAlign(AlignTop)
{
}

/*!
    \qmlproperty int QtQuick::Grid::columns

    This property holds the number of columns in the grid. The default
    number of columns is 4.

    If the grid does not have enough items to fill the specified
    number of columns, some columns will be of zero width.
*/

/*!
    \qmlproperty int QtQuick::Grid::rows
    This property holds the number of rows in the grid.

    If the grid does not have enough items to fill the specified
    number of rows, some rows will be of zero width.
*/

void QQuickGrid::setColumns(const int columns)
{
    if (columns == m_columns)
        return;
    m_columns = columns;
    prePositioning();
    emit columnsChanged();
}

void QQuickGrid::setRows(const int rows)
{
    if (rows == m_rows)
        return;
    m_rows = rows;
    prePositioning();
    emit rowsChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Grid::flow
    This property holds the flow of the layout.

    Possible values are:

    \list
    \li Grid.LeftToRight (default) - Items are positioned next to
       each other in the \l layoutDirection, then wrapped to the next line.
    \li Grid.TopToBottom - Items are positioned next to each
       other from top to bottom, then wrapped to the next column.
    \endlist
*/
QQuickGrid::Flow QQuickGrid::flow() const
{
    return m_flow;
}

void QQuickGrid::setFlow(Flow flow)
{
    if (m_flow != flow) {
        m_flow = flow;
        prePositioning();
        emit flowChanged();
    }
}

/*!
    \qmlproperty qreal QtQuick::Grid::rowSpacing

    This property holds the spacing in pixels between rows.

    If this property is not set, then spacing is used for the row spacing.

    By default this property is not set.

    \sa columnSpacing
    \since 5.0
*/
void QQuickGrid::setRowSpacing(const qreal rowSpacing)
{
    if (rowSpacing == m_rowSpacing)
        return;
    m_rowSpacing = rowSpacing;
    m_useRowSpacing = true;
    prePositioning();
    emit rowSpacingChanged();
}

/*!
    \qmlproperty qreal QtQuick::Grid::columnSpacing

    This property holds the spacing in pixels between columns.

    If this property is not set, then spacing is used for the column spacing.

    By default this property is not set.

    \sa rowSpacing
    \since 5.0
*/
void QQuickGrid::setColumnSpacing(const qreal columnSpacing)
{
    if (columnSpacing == m_columnSpacing)
        return;
    m_columnSpacing = columnSpacing;
    m_useColumnSpacing = true;
    prePositioning();
    emit columnSpacingChanged();
}

/*!
    \qmlproperty enumeration QtQuick::Grid::layoutDirection

    This property holds the layout direction of the layout.

    Possible values are:

    \list
    \li Qt.LeftToRight (default) - Items are positioned from the top to bottom,
    and left to right. The flow direction is dependent on the
    \l Grid::flow property.
    \li Qt.RightToLeft - Items are positioned from the top to bottom,
    and right to left. The flow direction is dependent on the
    \l Grid::flow property.
    \endlist

    \sa Flow::layoutDirection, Row::layoutDirection, {Qt Quick Examples - Right to Left}
*/
Qt::LayoutDirection QQuickGrid::layoutDirection() const
{
    return QQuickBasePositionerPrivate::getLayoutDirection(this);
}

void QQuickGrid::setLayoutDirection(Qt::LayoutDirection layoutDirection)
{
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate*>(QQuickBasePositionerPrivate::get(this));
    if (d->layoutDirection != layoutDirection) {
        d->layoutDirection = layoutDirection;
        emit layoutDirectionChanged();
        d->effectiveLayoutDirectionChange();
    }
}

/*!
    \qmlproperty enumeration QtQuick::Grid::effectiveLayoutDirection
    This property holds the effective layout direction of the grid.

    When using the attached property \l {LayoutMirroring::enabled}{LayoutMirroring::enabled} for locale layouts,
    the visual layout direction of the grid positioner will be mirrored. However, the
    property \l {Grid::layoutDirection}{layoutDirection} will remain unchanged.

    \sa Grid::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/
Qt::LayoutDirection QQuickGrid::effectiveLayoutDirection() const
{
    return QQuickBasePositionerPrivate::getEffectiveLayoutDirection(this);
}

/*!
    \qmlproperty enumeration QtQuick::Grid::horizontalItemAlignment
    \qmlproperty enumeration QtQuick::Grid::verticalItemAlignment
    \qmlproperty enumeration QtQuick::Grid::effectiveHorizontalItemAlignment

    Sets the horizontal and vertical alignment of items in the Grid. By default,
    the items are vertically aligned to the top. Horizontal
    alignment follows the layoutDirection of the Grid, for example when having a layoutDirection
    from LeftToRight, the items will be aligned on the left.

    The valid values for \c horizontalItemAlignment are, \c Grid.AlignLeft, \c Grid.AlignRight and
    \c Grid.AlignHCenter.

    The valid values for \c verticalItemAlignment are \c Grid.AlignTop, \c Grid.AlignBottom
    and \c Grid.AlignVCenter.

    The below images show three examples of how to align items.

    \table
    \row
        \li
        \li \inlineimage gridLayout_aligntopleft.png
        \li \inlineimage gridLayout_aligntop.png
        \li \inlineimage gridLayout_aligncenter.png
    \row
        \li Horizontal alignment
        \li AlignLeft
        \li AlignHCenter
        \li AlignHCenter
    \row
        \li Vertical alignment
        \li AlignTop
        \li AlignTop
        \li AlignVCenter
    \endtable


    When mirroring the layout using either the attached property LayoutMirroring::enabled or
    by setting the layoutDirection, the horizontal alignment of items will be mirrored as well.
    However, the property \c horizontalItemAlignment will remain unchanged.
    To query the effective horizontal alignment of items, use the read-only property
    \c effectiveHorizontalItemAlignment.

    \sa Grid::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/
QQuickGrid::HAlignment QQuickGrid::hItemAlign() const
{
    return m_hItemAlign;
}
void QQuickGrid::setHItemAlign(HAlignment align)
{
    if (m_hItemAlign != align) {
        m_hItemAlign = align;
        prePositioning();
        emit horizontalAlignmentChanged(align);
        emit effectiveHorizontalAlignmentChanged(effectiveHAlign());
    }
}

QQuickGrid::HAlignment QQuickGrid::effectiveHAlign() const
{
    HAlignment effectiveAlignment = m_hItemAlign;
    if (effectiveLayoutDirection() == Qt::RightToLeft) {
        switch (hItemAlign()) {
        case AlignLeft:
            effectiveAlignment = AlignRight;
            break;
        case AlignRight:
            effectiveAlignment = AlignLeft;
            break;
        default:
            break;
        }
    }
    return effectiveAlignment;
}


QQuickGrid::VAlignment QQuickGrid::vItemAlign() const
{
    return m_vItemAlign;
}
void QQuickGrid::setVItemAlign(VAlignment align)
{
    if (m_vItemAlign != align) {
        m_vItemAlign = align;
        prePositioning();
        emit verticalAlignmentChanged(align);
    }
}

void QQuickGrid::doPositioning(QSizeF *contentSize)
{
    //Precondition: All items in the positioned list have a valid item pointer and should be positioned
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate*>(QQuickBasePositionerPrivate::get(this));
    int c = m_columns;
    int r = m_rows;
    int numVisible = positionedItems.count();

    if (m_columns <= 0 && m_rows <= 0) {
        c = 4;
        r = (numVisible+3)/4;
    } else if (m_rows <= 0) {
        r = (numVisible+(m_columns-1))/m_columns;
    } else if (m_columns <= 0) {
        c = (numVisible+(m_rows-1))/m_rows;
    }

    if (r == 0 || c == 0) {
        contentSize->setHeight(topPadding() + bottomPadding());
        contentSize->setWidth(leftPadding() + rightPadding());
        return; //Nothing else to do
    }

    QList<qreal> maxColWidth;
    QList<qreal> maxRowHeight;
    int childIndex =0;
    if (m_flow == LeftToRight) {
        for (int i = 0; i < r; i++) {
            for (int j = 0; j < c; j++) {
                if (j == 0)
                    maxRowHeight << 0;
                if (i == 0)
                    maxColWidth << 0;

                if (childIndex == numVisible)
                    break;

                const PositionedItem &child = positionedItems.at(childIndex++);
                if (child.item->width() > maxColWidth[j])
                    maxColWidth[j] = child.item->width();
                if (child.item->height() > maxRowHeight[i])
                    maxRowHeight[i] = child.item->height();
            }
        }
    } else {
        for (int j = 0; j < c; j++) {
            for (int i = 0; i < r; i++) {
                if (j == 0)
                    maxRowHeight << 0;
                if (i == 0)
                    maxColWidth << 0;

                if (childIndex == numVisible)
                    break;

                const PositionedItem &child = positionedItems.at(childIndex++);
                if (child.item->width() > maxColWidth[j])
                    maxColWidth[j] = child.item->width();
                if (child.item->height() > maxRowHeight[i])
                    maxRowHeight[i] = child.item->height();
            }
        }
    }

    qreal columnSpacing = m_useColumnSpacing ? m_columnSpacing : spacing();
    qreal rowSpacing = m_useRowSpacing ? m_rowSpacing : spacing();

    qreal widthSum = 0;
    for (int j = 0; j < maxColWidth.size(); j++) {
        if (j)
            widthSum += columnSpacing;
        widthSum += maxColWidth[j];
    }
    widthSum += leftPadding() + rightPadding();

    qreal heightSum = 0;
    for (int i = 0; i < maxRowHeight.size(); i++) {
        if (i)
            heightSum += rowSpacing;
        heightSum += maxRowHeight[i];
    }
    heightSum += topPadding() + bottomPadding();

    contentSize->setHeight(heightSum);
    contentSize->setWidth(widthSum);

    int end = 0;
    if (widthValid())
        end = width();
    else
        end = widthSum;

    qreal xoffset = leftPadding();
    if (!d->isLeftToRight())
        xoffset = end - rightPadding();
    qreal yoffset = topPadding();
    int curRow =0;
    int curCol =0;
    for (int i = 0; i < positionedItems.count(); ++i) {
        PositionedItem &child = positionedItems[i];
        qreal childXOffset = xoffset;

        if (effectiveHAlign() == AlignRight)
            childXOffset += maxColWidth[curCol] - child.item->width();
        else if (hItemAlign() == AlignHCenter)
            childXOffset += (maxColWidth[curCol] - child.item->width())/2.0;

        if (!d->isLeftToRight())
            childXOffset -= maxColWidth[curCol];

        qreal alignYOffset = yoffset;
        if (m_vItemAlign == AlignVCenter)
            alignYOffset += (maxRowHeight[curRow] - child.item->height())/2.0;
        else if (m_vItemAlign == AlignBottom)
            alignYOffset += maxRowHeight[curRow] - child.item->height();

        positionItem(childXOffset, alignYOffset, &child);
        child.updatePadding(leftPadding(), topPadding(), rightPadding(), bottomPadding());

        if (m_flow == LeftToRight) {
            if (d->isLeftToRight())
                xoffset += maxColWidth[curCol]+columnSpacing;
            else
                xoffset -= maxColWidth[curCol]+columnSpacing;
            curCol++;
            curCol %= c;
            if (!curCol) {
                yoffset += maxRowHeight[curRow]+rowSpacing;
                if (d->isLeftToRight())
                    xoffset = leftPadding();
                else
                    xoffset = end - rightPadding();
                curRow++;
                if (curRow>=r)
                    break;
            }
        } else {
            yoffset += maxRowHeight[curRow]+rowSpacing;
            curRow++;
            curRow %= r;
            if (!curRow) {
                if (d->isLeftToRight())
                    xoffset += maxColWidth[curCol]+columnSpacing;
                else
                    xoffset -= maxColWidth[curCol]+columnSpacing;
                yoffset = topPadding();
                curCol++;
                if (curCol>=c)
                    break;
            }
        }
    }
}

void QQuickGrid::reportConflictingAnchors()
{
    QQuickBasePositionerPrivate *d = static_cast<QQuickBasePositionerPrivate*>(QQuickBasePositionerPrivate::get(this));
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        const PositionedItem &child = positionedItems.at(ii);
        if (child.item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(static_cast<QQuickItem *>(child.item))->_anchors;
            if (anchors && (anchors->usedAnchors() || anchors->fill() || anchors->centerIn())) {
                d->anchorConflict = true;
                break;
            }
        }
    }
    if (d->anchorConflict)
        qmlInfo(this) << "Cannot specify anchors for items inside Grid." << " Grid will not function.";
}

/*!
    \qmltype Flow
    \instantiates QQuickFlow
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-positioners
    \brief Positions its children side by side, wrapping as necessary

    The Flow item positions its child items like words on a page, wrapping them
    to create rows or columns of items.

    Below is a Flow that contains various \l Text items:

    \snippet qml/flow.qml flow item

    The Flow item automatically positions the child \l Text items side by
    side, wrapping as necessary:

    \image qml-flow-snippet.png

    If an item within a Flow is not \l {Item::}{visible}, or if it has a width or
    height of 0, the item will not be laid out and it will not be visible within the
    Flow. Also, since a Flow automatically positions its children, a child
    item within a Flow should not set its \l {Item::x}{x} or \l {Item::y}{y} positions
    or anchor itself with any of the \l {Item::anchors}{anchor} properties.

    For more information on using Flow and other related positioner-types, see
    \l{Item Positioners}.

  \sa Column, Row, Grid, Positioner, {Qt Quick Examples - Positioners}
*/
/*!
    \since 5.6
    \qmlproperty real QtQuick::Flow::padding
    \qmlproperty real QtQuick::Flow::topPadding
    \qmlproperty real QtQuick::Flow::leftPadding
    \qmlproperty real QtQuick::Flow::bottomPadding
    \qmlproperty real QtQuick::Flow::rightPadding

    These properties hold the padding around the content.
*/
/*!
    \qmlproperty Transition QtQuick::Flow::populate

    This property holds the transition to be run for items that are part of
    this positioner at the time of its creation. The transition is run when the positioner
    is first created.

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Flow::add

    This property holds the transition to be run for items that are added to this
    positioner. For a positioner, this applies to:

    \list
    \li Items that are created or reparented as a child of the positioner after the
        positioner has been created
    \li Child items that change their \l Item::visible property from false to true, and thus
       are now visible
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being added. See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note This transition is not applied to the items that are already part of the positioner
    at the time of its creation. In this case, the \l populate transition is applied instead.

    \sa populate, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
    \qmlproperty Transition QtQuick::Flow::move

    This property holds the transition to run for items that have moved within the
    positioner. For a positioner, this applies to:

    \list
    \li Child items that move when they are displaced due to the addition, removal or
       rearrangement of other items in the positioner
    \li Child items that are repositioned due to the resizing of other items in the positioner
    \endlist

    The transition can use the \l ViewTransition property to access more details about
    the item that is being moved. Note, however, that for this move transition, the
    ViewTransition.targetIndexes and ViewTransition.targetItems lists are only set when
    this transition is triggered by the addition of other items in the positioner; in other
    cases, these lists will be empty.  See the \l ViewTransition documentation for more details
    and examples on using these transitions.

    \note In \l {Qt Quick 1}, this transition was applied to all items that were part of the
    positioner at the time of its creation. From \l {Qt Quick}{QtQuick 2} onwards, positioners apply the
    \l populate transition to these items instead.

    \sa add, ViewTransition, {Qt Quick Examples - Positioners}
*/
/*!
  \qmlproperty real QtQuick::Flow::spacing

  spacing is the amount in pixels left empty between each adjacent
  item, and defaults to 0.

  \sa Grid::spacing
*/

class QQuickFlowPrivate : public QQuickBasePositionerPrivate
{
    Q_DECLARE_PUBLIC(QQuickFlow)

public:
    QQuickFlowPrivate()
        : QQuickBasePositionerPrivate(), flow(QQuickFlow::LeftToRight)
    {}

    void effectiveLayoutDirectionChange()
    {
        Q_Q(QQuickFlow);
        // Don't postpone, as it might be the only trigger for visible changes.
        q->prePositioning();
        emit q->effectiveLayoutDirectionChanged();
    }

    QQuickFlow::Flow flow;
};

QQuickFlow::QQuickFlow(QQuickItem *parent)
: QQuickBasePositioner(*(new QQuickFlowPrivate), Both, parent)
{
    Q_D(QQuickFlow);
    // Flow layout requires relayout if its own size changes too.
    d->addItemChangeListener(d, QQuickItemPrivate::Geometry);
}

/*!
    \qmlproperty enumeration QtQuick::Flow::flow
    This property holds the flow of the layout.

    Possible values are:

    \list
    \li Flow.LeftToRight (default) - Items are positioned next to
    to each other according to the \l layoutDirection until the width of the Flow
    is exceeded, then wrapped to the next line.
    \li Flow.TopToBottom - Items are positioned next to each
    other from top to bottom until the height of the Flow is exceeded,
    then wrapped to the next column.
    \endlist
*/
QQuickFlow::Flow QQuickFlow::flow() const
{
    Q_D(const QQuickFlow);
    return d->flow;
}

void QQuickFlow::setFlow(Flow flow)
{
    Q_D(QQuickFlow);
    if (d->flow != flow) {
        d->flow = flow;
        prePositioning();
        emit flowChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::Flow::layoutDirection

    This property holds the layout direction of the layout.

    Possible values are:

    \list
    \li Qt.LeftToRight (default) - Items are positioned from the top to bottom,
    and left to right. The flow direction is dependent on the
    \l Flow::flow property.
    \li Qt.RightToLeft - Items are positioned from the top to bottom,
    and right to left. The flow direction is dependent on the
    \l Flow::flow property.
    \endlist

    \sa Grid::layoutDirection, Row::layoutDirection, {Qt Quick Examples - Right to Left}
*/

Qt::LayoutDirection QQuickFlow::layoutDirection() const
{
    Q_D(const QQuickFlow);
    return d->layoutDirection;
}

void QQuickFlow::setLayoutDirection(Qt::LayoutDirection layoutDirection)
{
    Q_D(QQuickFlow);
    if (d->layoutDirection != layoutDirection) {
        d->layoutDirection = layoutDirection;
        emit layoutDirectionChanged();
        d->effectiveLayoutDirectionChange();
    }
}

/*!
    \qmlproperty enumeration QtQuick::Flow::effectiveLayoutDirection
    This property holds the effective layout direction of the flow.

    When using the attached property \l {LayoutMirroring::enabled}{LayoutMirroring::enabled} for locale layouts,
    the visual layout direction of the grid positioner will be mirrored. However, the
    property \l {Flow::layoutDirection}{layoutDirection} will remain unchanged.

    \sa Flow::layoutDirection, {LayoutMirroring}{LayoutMirroring}
*/

Qt::LayoutDirection QQuickFlow::effectiveLayoutDirection() const
{
    return QQuickBasePositionerPrivate::getEffectiveLayoutDirection(this);
}

void QQuickFlow::doPositioning(QSizeF *contentSize)
{
    //Precondition: All items in the positioned list have a valid item pointer and should be positioned
    Q_D(QQuickFlow);

    qreal hoffset1 = leftPadding();
    qreal hoffset2 = rightPadding();
    if (!d->isLeftToRight())
        qSwap(hoffset1, hoffset2);
    qreal hoffset = hoffset1;
    const qreal voffset1 = topPadding();
    qreal voffset = voffset1;
    qreal linemax = 0;
    QList<qreal> hoffsets;
    contentSize->setWidth(qMax(contentSize->width(), hoffset1 + hoffset2));
    contentSize->setHeight(qMax(contentSize->height(), voffset + bottomPadding()));

    for (int i = 0; i < positionedItems.count(); ++i) {
        PositionedItem &child = positionedItems[i];

        if (d->flow == LeftToRight)  {
            if (widthValid() && hoffset != hoffset1 && hoffset + child.item->width() + hoffset2 > width()) {
                hoffset = hoffset1;
                voffset += linemax + spacing();
                linemax = 0;
            }
        } else {
            if (heightValid() && voffset != voffset1 && voffset + child.item->height() + bottomPadding() > height()) {
                voffset = voffset1;
                hoffset += linemax + spacing();
                linemax = 0;
            }
        }

        if (d->isLeftToRight()) {
            positionItem(hoffset, voffset, &child);
            child.updatePadding(leftPadding(), topPadding(), rightPadding(), bottomPadding());
        } else {
            hoffsets << hoffset;
            positionItemY(voffset, &child);
            child.topPadding = topPadding();
            child.bottomPadding = bottomPadding();
        }

        contentSize->setWidth(qMax(contentSize->width(), hoffset + child.item->width() + hoffset2));
        contentSize->setHeight(qMax(contentSize->height(), voffset + child.item->height() + bottomPadding()));

        if (d->flow == LeftToRight)  {
            hoffset += child.item->width();
            hoffset += spacing();
            linemax = qMax(linemax, child.item->height());
        } else {
            voffset += child.item->height();
            voffset += spacing();
            linemax = qMax(linemax, child.item->width());
        }
    }

    if (d->isLeftToRight())
        return;

    qreal end;
    if (widthValid())
        end = width();
    else
        end = contentSize->width();
    int acc = 0;
    for (int i = 0; i < positionedItems.count(); ++i) {
        PositionedItem &child = positionedItems[i];
        hoffset = end - hoffsets[acc++] - child.item->width();
        positionItemX(hoffset, &child);
        child.leftPadding = leftPadding();
        child.rightPadding = rightPadding();
    }
}

void QQuickFlow::reportConflictingAnchors()
{
    Q_D(QQuickFlow);
    for (int ii = 0; ii < positionedItems.count(); ++ii) {
        const PositionedItem &child = positionedItems.at(ii);
        if (child.item) {
            QQuickAnchors *anchors = QQuickItemPrivate::get(static_cast<QQuickItem *>(child.item))->_anchors;
            if (anchors && (anchors->usedAnchors() || anchors->fill() || anchors->centerIn())) {
                d->anchorConflict = true;
                break;
            }
        }
    }
    if (d->anchorConflict)
        qmlInfo(this) << "Cannot specify anchors for items inside Flow." << " Flow will not function.";
}

QT_END_NAMESPACE
