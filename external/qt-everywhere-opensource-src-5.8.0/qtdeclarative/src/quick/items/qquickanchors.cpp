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

#include "qquickanchors_p_p.h"

#include "qquickitem_p.h"

#include <qqmlinfo.h>

QT_BEGIN_NAMESPACE

static Q_ALWAYS_INLINE QQuickItem *readParentItem(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->parentItem;
}

static Q_ALWAYS_INLINE qreal readX(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->x;
}

static Q_ALWAYS_INLINE qreal readY(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->y;
}

static Q_ALWAYS_INLINE qreal readWidth(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->width;
}

static Q_ALWAYS_INLINE qreal readHeight(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->height;
}

static Q_ALWAYS_INLINE qreal readBaselineOffset(const QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->baselineOffset;
}

//TODO: should we cache relationships, so we don't have to check each time (parent-child or sibling)?
//TODO: support non-parent, non-sibling (need to find lowest common ancestor)

static inline qreal hcenter(const QQuickItem *item)
{
    qreal width = readWidth(item);
    if (QQuickAnchors *anchors = QQuickItemPrivate::get(item)->_anchors) {
        if (!QQuickAnchorsPrivate::get(anchors)->centerAligned)
            return width / 2;
    }
    int iw = width;
    if (iw % 2)
        return (width + 1) / 2;
    else
        return width / 2;
}

static inline qreal vcenter(const QQuickItem *item)
{
    qreal height = readHeight(item);
    if (QQuickAnchors *anchors = QQuickItemPrivate::get(item)->_anchors) {
        if (!QQuickAnchorsPrivate::get(anchors)->centerAligned)
            return height / 2;
    }
    int ih = height;
    if (ih % 2)
        return (height + 1) / 2;
    else
        return height / 2;
}

//local position
static inline qreal position(const QQuickItem *item, QQuickAnchors::Anchor anchorLine)
{
    qreal ret = 0.0;
    switch (anchorLine) {
    case QQuickAnchors::LeftAnchor:
        ret = readX(item);
        break;
    case QQuickAnchors::RightAnchor:
        ret = readX(item) + readWidth(item);
        break;
    case QQuickAnchors::TopAnchor:
        ret = readY(item);
        break;
    case QQuickAnchors::BottomAnchor:
        ret = readY(item) + readHeight(item);
        break;
    case QQuickAnchors::HCenterAnchor:
        ret = readX(item) + hcenter(item);
        break;
    case QQuickAnchors::VCenterAnchor:
        ret = readY(item) + vcenter(item);
        break;
    case QQuickAnchors::BaselineAnchor:
        ret = readY(item) + readBaselineOffset(item);
        break;
    default:
        break;
    }

    return ret;
}

//position when origin is 0,0
static inline qreal adjustedPosition(QQuickItem *item, QQuickAnchors::Anchor anchorLine)
{
    qreal ret = 0.0;
    switch (anchorLine) {
    case QQuickAnchors::LeftAnchor:
        ret = 0.0;
        break;
    case QQuickAnchors::RightAnchor:
        ret = readWidth(item);
        break;
    case QQuickAnchors::TopAnchor:
        ret = 0.0;
        break;
    case QQuickAnchors::BottomAnchor:
        ret = readHeight(item);
        break;
    case QQuickAnchors::HCenterAnchor:
        ret = hcenter(item);
        break;
    case QQuickAnchors::VCenterAnchor:
        ret = vcenter(item);
        break;
    case QQuickAnchors::BaselineAnchor:
        ret = readBaselineOffset(item);
        break;
    default:
        break;
    }

    return ret;
}

QQuickAnchors::QQuickAnchors(QQuickItem *item, QObject *parent)
: QObject(*new QQuickAnchorsPrivate(item), parent)
{
}

QQuickAnchors::~QQuickAnchors()
{
    Q_D(QQuickAnchors);
    d->inDestructor = true;
    d->remDepend(d->fill);
    d->remDepend(d->centerIn);
    d->remDepend(d->leftAnchorItem);
    d->remDepend(d->rightAnchorItem);
    d->remDepend(d->topAnchorItem);
    d->remDepend(d->bottomAnchorItem);
    d->remDepend(d->vCenterAnchorItem);
    d->remDepend(d->hCenterAnchorItem);
    d->remDepend(d->baselineAnchorItem);
}

void QQuickAnchorsPrivate::fillChanged()
{
    Q_Q(QQuickAnchors);
    if (!fill || !isItemComplete())
        return;

    if (updatingFill < 2) {
        ++updatingFill;

        qreal horizontalMargin = q->mirrored() ? rightMargin : leftMargin;

        if (fill == readParentItem(item)) {                         //child-parent
            setItemPos(QPointF(horizontalMargin, topMargin));
        } else if (readParentItem(fill) == readParentItem(item)) {   //siblings
            setItemPos(QPointF(readX(fill)+horizontalMargin, readY(fill) + topMargin));
        }
        setItemSize(QSizeF(readWidth(fill) - leftMargin - rightMargin,
                           readHeight(fill) - topMargin - bottomMargin));

        --updatingFill;
    } else {
        // ### Make this certain :)
        qmlInfo(item) << QQuickAnchors::tr("Possible anchor loop detected on fill.");
    }

}

void QQuickAnchorsPrivate::centerInChanged()
{
    Q_Q(QQuickAnchors);
    if (!centerIn || fill || !isItemComplete())
        return;

    if (updatingCenterIn < 2) {
        ++updatingCenterIn;

        qreal effectiveHCenterOffset = q->mirrored() ? -hCenterOffset : hCenterOffset;
        if (centerIn == readParentItem(item)) {
            QPointF p(hcenter(readParentItem(item)) - hcenter(item) + effectiveHCenterOffset,
                      vcenter(readParentItem(item)) - vcenter(item) + vCenterOffset);
            setItemPos(p);

        } else if (readParentItem(centerIn) == readParentItem(item)) {
            QPointF p(centerIn->x() + hcenter(centerIn) - hcenter(item) + effectiveHCenterOffset,
                      centerIn->y() + vcenter(centerIn) - vcenter(item) + vCenterOffset);
            setItemPos(p);
        }

        --updatingCenterIn;
    } else {
        // ### Make this certain :)
        qmlInfo(item) << QQuickAnchors::tr("Possible anchor loop detected on centerIn.");
    }
}

void QQuickAnchorsPrivate::clearItem(QQuickItem *item)
{
    if (!item)
        return;
    if (fill == item)
        fill = 0;
    if (centerIn == item)
        centerIn = 0;
    if (leftAnchorItem == item) {
        leftAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::LeftAnchor;
    }
    if (rightAnchorItem == item) {
        rightAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::RightAnchor;
    }
    if (topAnchorItem == item) {
        topAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::TopAnchor;
    }
    if (bottomAnchorItem == item) {
        bottomAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::BottomAnchor;
    }
    if (vCenterAnchorItem == item) {
        vCenterAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::VCenterAnchor;
    }
    if (hCenterAnchorItem == item) {
        hCenterAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::HCenterAnchor;
    }
    if (baselineAnchorItem == item) {
        baselineAnchorItem = 0;
        usedAnchors &= ~QQuickAnchors::BaselineAnchor;
    }
}

QQuickGeometryChange QQuickAnchorsPrivate::calculateDependency(QQuickItem *controlItem)
{
    QQuickGeometryChange dependency;

    if (!controlItem || inDestructor)
        return dependency;

    if (fill == controlItem) {
        if (controlItem == readParentItem(item))
            dependency.setSizeChange(true);
        else    //sibling
            dependency.setAllChanged(true);
        return dependency;  //exit early
    }

    if (centerIn == controlItem) {
        if (controlItem == readParentItem(item))
            dependency.setSizeChange(true);
        else    //sibling
            dependency.setAllChanged(true);
        return dependency;  //exit early
    }

    if ((usedAnchors & QQuickAnchors::LeftAnchor && leftAnchorItem == controlItem) ||
        (usedAnchors & QQuickAnchors::RightAnchor && rightAnchorItem == controlItem) ||
        (usedAnchors & QQuickAnchors::HCenterAnchor && hCenterAnchorItem == controlItem)) {
        if (controlItem == readParentItem(item))
            dependency.setWidthChange(true);
        else    //sibling
            dependency.setHorizontalChange(true);
    }

    if ((usedAnchors & QQuickAnchors::TopAnchor && topAnchorItem == controlItem) ||
        (usedAnchors & QQuickAnchors::BottomAnchor && bottomAnchorItem == controlItem) ||
        (usedAnchors & QQuickAnchors::VCenterAnchor && vCenterAnchorItem == controlItem) ||
        (usedAnchors & QQuickAnchors::BaselineAnchor && baselineAnchorItem == controlItem)) {
        if (controlItem == readParentItem(item))
            dependency.setHeightChange(true);
        else    //sibling
            dependency.setVerticalChange(true);
    }

    return dependency;
}

void QQuickAnchorsPrivate::addDepend(QQuickItem *item)
{
    if (!item || !componentComplete)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    p->updateOrAddGeometryChangeListener(this, calculateDependency(item));
}

void QQuickAnchorsPrivate::remDepend(QQuickItem *item)
{
    if (!item || !componentComplete)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    p->updateOrRemoveGeometryChangeListener(this, calculateDependency(item));
}

bool QQuickAnchors::mirrored()
{
    Q_D(QQuickAnchors);
    return QQuickItemPrivate::get(d->item)->effectiveLayoutMirror;
}

bool QQuickAnchors::alignWhenCentered() const
{
    Q_D(const QQuickAnchors);
    return d->centerAligned;
}

void QQuickAnchors::setAlignWhenCentered(bool aligned)
{
    Q_D(QQuickAnchors);
    if (aligned == d->centerAligned)
        return;
    d->centerAligned = aligned;
    emit centerAlignedChanged();
    if (d->centerIn) {
        d->centerInChanged();
    } else {
        if (d->usedAnchors & QQuickAnchors::VCenterAnchor)
            d->updateVerticalAnchors();
        else if (d->usedAnchors & QQuickAnchors::HCenterAnchor)
            d->updateHorizontalAnchors();
    }
}

bool QQuickAnchorsPrivate::isItemComplete() const
{
    return componentComplete;
}

void QQuickAnchors::classBegin()
{
    Q_D(QQuickAnchors);
    d->componentComplete = false;
}

void QQuickAnchors::componentComplete()
{
    Q_D(QQuickAnchors);
    d->componentComplete = true;
}

void QQuickAnchorsPrivate::setItemHeight(qreal v)
{
    updatingMe = true;
    item->setHeight(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::setItemWidth(qreal v)
{
    updatingMe = true;
    item->setWidth(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::setItemX(qreal v)
{
    updatingMe = true;
    item->setX(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::setItemY(qreal v)
{
    updatingMe = true;
    item->setY(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::setItemPos(const QPointF &v)
{
    updatingMe = true;
    item->setPosition(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::setItemSize(const QSizeF &v)
{
    updatingMe = true;
    item->setSize(v);
    updatingMe = false;
}

void QQuickAnchorsPrivate::updateMe()
{
    if (updatingMe) {
        updatingMe = false;
        return;
    }

    update();
}

void QQuickAnchorsPrivate::updateOnComplete()
{
    //optimization to only set initial dependencies once, at completion time
    QQuickItem *dependencies[9];
    dependencies[0] = fill;
    dependencies[1] = centerIn;
    dependencies[2] = leftAnchorItem;
    dependencies[3] = rightAnchorItem;
    dependencies[4] = hCenterAnchorItem;
    dependencies[5] = topAnchorItem;
    dependencies[6] = bottomAnchorItem;
    dependencies[7] = vCenterAnchorItem;
    dependencies[8] = baselineAnchorItem;

    std::sort(dependencies, dependencies + 9);

    QQuickItem *lastDependency = 0;
    for (int i = 0; i < 9; ++i) {
        QQuickItem *dependency = dependencies[i];
        if (lastDependency != dependency) {
            addDepend(dependency);
            lastDependency = dependency;
        }
    }

    update();
}


void QQuickAnchorsPrivate::update()
{
    if (!isItemComplete())
        return;

    if (fill) {
        fillChanged();
    } else if (centerIn) {
        centerInChanged();
    } else {
        if (usedAnchors & QQuickAnchors::Horizontal_Mask)
            updateHorizontalAnchors();
        if (usedAnchors & QQuickAnchors::Vertical_Mask)
            updateVerticalAnchors();
    }
}

void QQuickAnchorsPrivate::itemGeometryChanged(QQuickItem *, QQuickGeometryChange change, const QRectF &)
{
    if (!isItemComplete())
        return;

    if (fill) {
        fillChanged();
    } else if (centerIn) {
        centerInChanged();
    } else {
        if ((usedAnchors & QQuickAnchors::Horizontal_Mask) && change.horizontalChange())
            updateHorizontalAnchors();
        if ((usedAnchors & QQuickAnchors::Vertical_Mask) && change.verticalChange())
            updateVerticalAnchors();
    }
}

QQuickItem *QQuickAnchors::fill() const
{
    Q_D(const QQuickAnchors);
    return d->fill;
}

void QQuickAnchors::setFill(QQuickItem *f)
{
    Q_D(QQuickAnchors);
    if (d->fill == f)
        return;

    if (!f) {
        QQuickItem *oldFill = d->fill;
        d->fill = f;
        d->remDepend(oldFill);
        emit fillChanged();
        return;
    }
    if (f != readParentItem(d->item) && readParentItem(f) != readParentItem(d->item)){
        qmlInfo(d->item) << tr("Cannot anchor to an item that isn't a parent or sibling.");
        return;
    }
    QQuickItem *oldFill = d->fill;
    d->fill = f;
    d->remDepend(oldFill);
    d->addDepend(d->fill);
    emit fillChanged();
    d->fillChanged();
}

void QQuickAnchors::resetFill()
{
    setFill(0);
}

QQuickItem *QQuickAnchors::centerIn() const
{
    Q_D(const QQuickAnchors);
    return d->centerIn;
}

void QQuickAnchors::setCenterIn(QQuickItem* c)
{
    Q_D(QQuickAnchors);
    if (d->centerIn == c)
        return;

    if (!c) {
        QQuickItem *oldCI = d->centerIn;
        d->centerIn = c;
        d->remDepend(oldCI);
        emit centerInChanged();
        return;
    }
    if (c != readParentItem(d->item) && readParentItem(c) != readParentItem(d->item)){
        qmlInfo(d->item) << tr("Cannot anchor to an item that isn't a parent or sibling.");
        return;
    }
    QQuickItem *oldCI = d->centerIn;
    d->centerIn = c;
    d->remDepend(oldCI);
    d->addDepend(d->centerIn);
    emit centerInChanged();
    d->centerInChanged();
}

void QQuickAnchors::resetCenterIn()
{
    setCenterIn(0);
}

bool QQuickAnchorsPrivate::calcStretch(QQuickItem *edge1Item,
                                       QQuickAnchors::Anchor edge1Line,
                                       QQuickItem *edge2Item,
                                       QQuickAnchors::Anchor edge2Line,
                                       qreal offset1,
                                       qreal offset2,
                                       QQuickAnchors::Anchor line,
                                       qreal &stretch)
{
    bool edge1IsParent = (edge1Item == readParentItem(item));
    bool edge2IsParent = (edge2Item == readParentItem(item));
    bool edge1IsSibling = (readParentItem(edge1Item) == readParentItem(item));
    bool edge2IsSibling = (readParentItem(edge2Item) == readParentItem(item));

    bool invalid = false;
    if ((edge2IsParent && edge1IsParent) || (edge2IsSibling && edge1IsSibling)) {
        stretch = (position(edge2Item, edge2Line) + offset2)
                    - (position(edge1Item, edge1Line) + offset1);
    } else if (edge2IsParent && edge1IsSibling) {
        stretch = (position(edge2Item, edge2Line) + offset2)
                    - (position(readParentItem(item), line)
                    + position(edge1Item, edge1Line) + offset1);
    } else if (edge2IsSibling && edge1IsParent) {
        stretch = (position(readParentItem(item), line) + position(edge2Item, edge2Line) + offset2)
                    - (position(edge1Item, edge1Line) + offset1);
    } else
        invalid = true;

    return invalid;
}

void QQuickAnchorsPrivate::updateVerticalAnchors()
{
    if (fill || centerIn || !isItemComplete())
        return;

    if (Q_UNLIKELY(updatingVerticalAnchor > 1)) {
        // ### Make this certain :)
        qmlInfo(item) << QQuickAnchors::tr("Possible anchor loop detected on vertical anchor.");
        return;
    }

    ++updatingVerticalAnchor;
    if (usedAnchors & QQuickAnchors::TopAnchor) {
        //Handle stretching
        bool invalid = true;
        qreal height = 0.0;
        if (usedAnchors & QQuickAnchors::BottomAnchor) {
            invalid = calcStretch(topAnchorItem, topAnchorLine,
                                  bottomAnchorItem, bottomAnchorLine,
                                  topMargin, -bottomMargin, QQuickAnchors::TopAnchor, height);
        } else if (usedAnchors & QQuickAnchors::VCenterAnchor) {
            invalid = calcStretch(topAnchorItem, topAnchorLine,
                                  vCenterAnchorItem, vCenterAnchorLine,
                                  topMargin, vCenterOffset, QQuickAnchors::TopAnchor, height);
            height *= 2;
        }
        if (!invalid)
            setItemHeight(height);

        //Handle top
        if (topAnchorItem == readParentItem(item)) {
            setItemY(adjustedPosition(topAnchorItem, topAnchorLine) + topMargin);
        } else if (readParentItem(topAnchorItem) == readParentItem(item)) {
            setItemY(position(topAnchorItem, topAnchorLine) + topMargin);
        }
    } else if (usedAnchors & QQuickAnchors::BottomAnchor) {
        //Handle stretching (top + bottom case is handled above)
        if (usedAnchors & QQuickAnchors::VCenterAnchor) {
            qreal height = 0.0;
            bool invalid = calcStretch(vCenterAnchorItem, vCenterAnchorLine,
                                       bottomAnchorItem, bottomAnchorLine,
                                       vCenterOffset, -bottomMargin, QQuickAnchors::TopAnchor,
                                       height);
            if (!invalid)
                setItemHeight(height*2);
        }

        //Handle bottom
        if (bottomAnchorItem == readParentItem(item)) {
            setItemY(adjustedPosition(bottomAnchorItem, bottomAnchorLine) - readHeight(item) - bottomMargin);
        } else if (readParentItem(bottomAnchorItem) == readParentItem(item)) {
            setItemY(position(bottomAnchorItem, bottomAnchorLine) - readHeight(item) - bottomMargin);
        }
    } else if (usedAnchors & QQuickAnchors::VCenterAnchor) {
        //(stetching handled above)

        //Handle vCenter
        if (vCenterAnchorItem == readParentItem(item)) {
            setItemY(adjustedPosition(vCenterAnchorItem, vCenterAnchorLine)
                     - vcenter(item) + vCenterOffset);
        } else if (readParentItem(vCenterAnchorItem) == readParentItem(item)) {
            setItemY(position(vCenterAnchorItem, vCenterAnchorLine) - vcenter(item) + vCenterOffset);
        }
    } else if (usedAnchors & QQuickAnchors::BaselineAnchor) {
        //Handle baseline
        if (baselineAnchorItem == readParentItem(item)) {
            setItemY(adjustedPosition(baselineAnchorItem, baselineAnchorLine)
                     - readBaselineOffset(item) + baselineOffset);
        } else if (readParentItem(baselineAnchorItem) == readParentItem(item)) {
            setItemY(position(baselineAnchorItem, baselineAnchorLine)
                     - readBaselineOffset(item) + baselineOffset);
        }
    }
    --updatingVerticalAnchor;
}

static inline QQuickAnchors::Anchor reverseAnchorLine(QQuickAnchors::Anchor anchorLine)
{
    if (anchorLine == QQuickAnchors::LeftAnchor) {
        return QQuickAnchors::RightAnchor;
    } else if (anchorLine == QQuickAnchors::RightAnchor) {
        return QQuickAnchors::LeftAnchor;
    } else {
        return anchorLine;
    }
}

void QQuickAnchorsPrivate::updateHorizontalAnchors()
{
    Q_Q(QQuickAnchors);
    if (fill || centerIn || !isItemComplete())
        return;

    if (updatingHorizontalAnchor < 3) {
        ++updatingHorizontalAnchor;
        qreal effectiveRightMargin, effectiveLeftMargin, effectiveHorizontalCenterOffset;
        QQuickItem *effectiveLeftItem, *effectiveRightItem, *effectiveHorizontalCenterItem;
        QQuickAnchors::Anchor effectiveLeftLine, effectiveRightLine, effectiveHorizontalCenterLine;
        QQuickAnchors::Anchor effectiveLeftAnchor, effectiveRightAnchor;
        if (q->mirrored()) {
            effectiveLeftAnchor = QQuickAnchors::RightAnchor;
            effectiveRightAnchor = QQuickAnchors::LeftAnchor;
            effectiveLeftItem = rightAnchorItem;
            effectiveLeftLine = reverseAnchorLine(rightAnchorLine);
            effectiveRightItem = leftAnchorItem;
            effectiveRightLine = reverseAnchorLine(leftAnchorLine);
            effectiveHorizontalCenterItem = hCenterAnchorItem;
            effectiveHorizontalCenterLine = reverseAnchorLine(hCenterAnchorLine);
            effectiveLeftMargin = rightMargin;
            effectiveRightMargin = leftMargin;
            effectiveHorizontalCenterOffset = -hCenterOffset;
        } else {
            effectiveLeftAnchor = QQuickAnchors::LeftAnchor;
            effectiveRightAnchor = QQuickAnchors::RightAnchor;
            effectiveLeftItem = leftAnchorItem;
            effectiveLeftLine = leftAnchorLine;
            effectiveRightItem = rightAnchorItem;
            effectiveRightLine = rightAnchorLine;
            effectiveHorizontalCenterItem = hCenterAnchorItem;
            effectiveHorizontalCenterLine = hCenterAnchorLine;
            effectiveLeftMargin = leftMargin;
            effectiveRightMargin = rightMargin;
            effectiveHorizontalCenterOffset = hCenterOffset;
        }

        if (usedAnchors & effectiveLeftAnchor) {
            //Handle stretching
            bool invalid = true;
            qreal width = 0.0;
            if (usedAnchors & effectiveRightAnchor) {
                invalid = calcStretch(effectiveLeftItem, effectiveLeftLine,
                                      effectiveRightItem, effectiveRightLine,
                                      effectiveLeftMargin, -effectiveRightMargin,
                                      QQuickAnchors::LeftAnchor, width);
            } else if (usedAnchors & QQuickAnchors::HCenterAnchor) {
                invalid = calcStretch(effectiveLeftItem, effectiveLeftLine,
                                      effectiveHorizontalCenterItem, effectiveHorizontalCenterLine,
                                      effectiveLeftMargin, effectiveHorizontalCenterOffset,
                                      QQuickAnchors::LeftAnchor, width);
                width *= 2;
            }
            if (!invalid)
                setItemWidth(width);

            //Handle left
            if (effectiveLeftItem == readParentItem(item)) {
                setItemX(adjustedPosition(effectiveLeftItem, effectiveLeftLine) + effectiveLeftMargin);
            } else if (readParentItem(effectiveLeftItem) == readParentItem(item)) {
                setItemX(position(effectiveLeftItem, effectiveLeftLine) + effectiveLeftMargin);
            }
        } else if (usedAnchors & effectiveRightAnchor) {
            //Handle stretching (left + right case is handled in updateLeftAnchor)
            if (usedAnchors & QQuickAnchors::HCenterAnchor) {
                qreal width = 0.0;
                bool invalid = calcStretch(effectiveHorizontalCenterItem,
                                           effectiveHorizontalCenterLine,
                                           effectiveRightItem,  effectiveRightLine,
                                           effectiveHorizontalCenterOffset, -effectiveRightMargin,
                                           QQuickAnchors::LeftAnchor, width);
                if (!invalid)
                    setItemWidth(width*2);
            }

            //Handle right
            if (effectiveRightItem == readParentItem(item)) {
                setItemX(adjustedPosition(effectiveRightItem, effectiveRightLine)
                         - readWidth(item) - effectiveRightMargin);
            } else if (readParentItem(effectiveRightItem) == readParentItem(item)) {
                setItemX(position(effectiveRightItem, effectiveRightLine)
                         - readWidth(item) - effectiveRightMargin);
            }
        } else if (usedAnchors & QQuickAnchors::HCenterAnchor) {
            //Handle hCenter
            if (effectiveHorizontalCenterItem == readParentItem(item)) {
                setItemX(adjustedPosition(effectiveHorizontalCenterItem, effectiveHorizontalCenterLine) - hcenter(item) + effectiveHorizontalCenterOffset);
            } else if (readParentItem(effectiveHorizontalCenterItem) == readParentItem(item)) {
                setItemX(position(effectiveHorizontalCenterItem, effectiveHorizontalCenterLine) - hcenter(item) + effectiveHorizontalCenterOffset);
            }
        }
        --updatingHorizontalAnchor;
    } else {
        // ### Make this certain :)
        qmlInfo(item) << QQuickAnchors::tr("Possible anchor loop detected on horizontal anchor.");
    }
}

QQuickAnchorLine QQuickAnchors::top() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->topAnchorItem, d->topAnchorLine);
}

void QQuickAnchors::setTop(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) ||
            (d->topAnchorItem == edge.item && d->topAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= TopAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~TopAnchor;
        return;
    }

    QQuickItem *oldTop = d->topAnchorItem;
    d->topAnchorItem = edge.item;
    d->topAnchorLine = edge.anchorLine;
    d->remDepend(oldTop);
    d->addDepend(d->topAnchorItem);
    emit topChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetTop()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~TopAnchor;
    d->remDepend(d->topAnchorItem);
    d->topAnchorItem = Q_NULLPTR;
    d->topAnchorLine = QQuickAnchors::InvalidAnchor;
    emit topChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::bottom() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->bottomAnchorItem, d->bottomAnchorLine);
}

void QQuickAnchors::setBottom(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) ||
            (d->bottomAnchorItem == edge.item && d->bottomAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= BottomAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~BottomAnchor;
        return;
    }

    QQuickItem *oldBottom = d->bottomAnchorItem;
    d->bottomAnchorItem = edge.item;
    d->bottomAnchorLine = edge.anchorLine;
    d->remDepend(oldBottom);
    d->addDepend(d->bottomAnchorItem);
    emit bottomChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetBottom()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~BottomAnchor;
    d->remDepend(d->bottomAnchorItem);
    d->bottomAnchorItem = Q_NULLPTR;
    d->bottomAnchorLine = QQuickAnchors::InvalidAnchor;
    emit bottomChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::verticalCenter() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->vCenterAnchorItem, d->vCenterAnchorLine);
}

void QQuickAnchors::setVerticalCenter(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) ||
            (d->vCenterAnchorItem == edge.item && d->vCenterAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= VCenterAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~VCenterAnchor;
        return;
    }

    QQuickItem *oldVCenter = d->vCenterAnchorItem;
    d->vCenterAnchorItem = edge.item;
    d->vCenterAnchorLine = edge.anchorLine;
    d->remDepend(oldVCenter);
    d->addDepend(d->vCenterAnchorItem);
    emit verticalCenterChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetVerticalCenter()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~VCenterAnchor;
    d->remDepend(d->vCenterAnchorItem);
    d->vCenterAnchorItem = Q_NULLPTR;
    d->vCenterAnchorLine = QQuickAnchors::InvalidAnchor;
    emit verticalCenterChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::baseline() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->baselineAnchorItem, d->baselineAnchorLine);
}

void QQuickAnchors::setBaseline(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) ||
            (d->baselineAnchorItem == edge.item && d->baselineAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= BaselineAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~BaselineAnchor;
        return;
    }

    QQuickItem *oldBaseline = d->baselineAnchorItem;
    d->baselineAnchorItem = edge.item;
    d->baselineAnchorLine = edge.anchorLine;
    d->remDepend(oldBaseline);
    d->addDepend(d->baselineAnchorItem);
    emit baselineChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetBaseline()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~BaselineAnchor;
    d->remDepend(d->baselineAnchorItem);
    d->baselineAnchorItem = Q_NULLPTR;
    d->baselineAnchorLine = QQuickAnchors::InvalidAnchor;
    emit baselineChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::left() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->leftAnchorItem, d->leftAnchorLine);
}

void QQuickAnchors::setLeft(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) ||
            (d->leftAnchorItem == edge.item && d->leftAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= LeftAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~LeftAnchor;
        return;
    }

    QQuickItem *oldLeft = d->leftAnchorItem;
    d->leftAnchorItem = edge.item;
    d->leftAnchorLine = edge.anchorLine;
    d->remDepend(oldLeft);
    d->addDepend(d->leftAnchorItem);
    emit leftChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetLeft()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~LeftAnchor;
    d->remDepend(d->leftAnchorItem);
    d->leftAnchorItem = Q_NULLPTR;
    d->leftAnchorLine = QQuickAnchors::InvalidAnchor;
    emit leftChanged();
    d->updateHorizontalAnchors();
}

QQuickAnchorLine QQuickAnchors::right() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->rightAnchorItem, d->rightAnchorLine);
}

void QQuickAnchors::setRight(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) ||
            (d->rightAnchorItem == edge.item && d->rightAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= RightAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~RightAnchor;
        return;
    }

    QQuickItem *oldRight = d->rightAnchorItem;
    d->rightAnchorItem = edge.item;
    d->rightAnchorLine = edge.anchorLine;
    d->remDepend(oldRight);
    d->addDepend(d->rightAnchorItem);
    emit rightChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetRight()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~RightAnchor;
    d->remDepend(d->rightAnchorItem);
    d->rightAnchorItem = Q_NULLPTR;
    d->rightAnchorLine = QQuickAnchors::InvalidAnchor;
    emit rightChanged();
    d->updateHorizontalAnchors();
}

QQuickAnchorLine QQuickAnchors::horizontalCenter() const
{
    Q_D(const QQuickAnchors);
    return QQuickAnchorLine(d->hCenterAnchorItem, d->hCenterAnchorLine);
}

void QQuickAnchors::setHorizontalCenter(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) ||
            (d->hCenterAnchorItem == edge.item && d->hCenterAnchorLine == edge.anchorLine))
        return;

    d->usedAnchors |= HCenterAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~HCenterAnchor;
        return;
    }

    QQuickItem *oldHCenter = d->hCenterAnchorItem;
    d->hCenterAnchorItem = edge.item;
    d->hCenterAnchorLine = edge.anchorLine;
    d->remDepend(oldHCenter);
    d->addDepend(d->hCenterAnchorItem);
    emit horizontalCenterChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetHorizontalCenter()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~HCenterAnchor;
    d->remDepend(d->hCenterAnchorItem);
    d->hCenterAnchorItem = Q_NULLPTR;
    d->hCenterAnchorLine = QQuickAnchors::InvalidAnchor;
    emit horizontalCenterChanged();
    d->updateHorizontalAnchors();
}

qreal QQuickAnchors::leftMargin() const
{
    Q_D(const QQuickAnchors);
    return d->leftMargin;
}

void QQuickAnchors::setLeftMargin(qreal offset)
{
    Q_D(QQuickAnchors);
    d->leftMarginExplicit = true;
    if (d->leftMargin == offset)
        return;
    d->leftMargin = offset;
    if (d->fill)
        d->fillChanged();
    else
        d->updateHorizontalAnchors();
    emit leftMarginChanged();
}

void QQuickAnchors::resetLeftMargin()
{
    Q_D(QQuickAnchors);
    d->leftMarginExplicit = false;
    if (d->leftMargin == d->margins)
        return;
    d->leftMargin = d->margins;
    if (d->fill)
        d->fillChanged();
    else
        d->updateHorizontalAnchors();
    emit leftMarginChanged();
}

qreal QQuickAnchors::rightMargin() const
{
    Q_D(const QQuickAnchors);
    return d->rightMargin;
}

void QQuickAnchors::setRightMargin(qreal offset)
{
    Q_D(QQuickAnchors);
    d->rightMarginExplicit = true;
    if (d->rightMargin == offset)
        return;
    d->rightMargin = offset;
    if (d->fill)
        d->fillChanged();
    else
        d->updateHorizontalAnchors();
    emit rightMarginChanged();
}

void QQuickAnchors::resetRightMargin()
{
    Q_D(QQuickAnchors);
    d->rightMarginExplicit = false;
    if (d->rightMargin == d->margins)
        return;
    d->rightMargin = d->margins;
    if (d->fill)
        d->fillChanged();
    else
        d->updateHorizontalAnchors();
    emit rightMarginChanged();
}

qreal QQuickAnchors::margins() const
{
    Q_D(const QQuickAnchors);
    return d->margins;
}

void QQuickAnchors::setMargins(qreal offset)
{
    Q_D(QQuickAnchors);
    if (d->margins == offset)
        return;
    d->margins = offset;

    bool updateHorizontal = false;
    bool updateVertical = false;

    if (!d->rightMarginExplicit && d->rightMargin != offset) {
        d->rightMargin = offset;
        updateHorizontal = true;
        emit rightMarginChanged();
    }
    if (!d->leftMarginExplicit && d->leftMargin != offset) {
        d->leftMargin = offset;
        updateHorizontal = true;
        emit leftMarginChanged();
    }
    if (!d->topMarginExplicit && d->topMargin != offset) {
        d->topMargin = offset;
        updateVertical = true;
        emit topMarginChanged();
    }
    if (!d->bottomMarginExplicit && d->bottomMargin != offset) {
        d->bottomMargin = offset;
        updateVertical = true;
        emit bottomMarginChanged();
    }

    if (d->fill) {
        if (updateHorizontal || updateVertical)
            d->fillChanged();
    } else {
        if (updateHorizontal)
            d->updateHorizontalAnchors();
        if (updateVertical)
            d->updateVerticalAnchors();
    }

    emit marginsChanged();
}

qreal QQuickAnchors::horizontalCenterOffset() const
{
    Q_D(const QQuickAnchors);
    return d->hCenterOffset;
}

void QQuickAnchors::setHorizontalCenterOffset(qreal offset)
{
    Q_D(QQuickAnchors);
    if (d->hCenterOffset == offset)
        return;
    d->hCenterOffset = offset;
    if (d->centerIn)
        d->centerInChanged();
    else
        d->updateHorizontalAnchors();
    emit horizontalCenterOffsetChanged();
}

qreal QQuickAnchors::topMargin() const
{
    Q_D(const QQuickAnchors);
    return d->topMargin;
}

void QQuickAnchors::setTopMargin(qreal offset)
{
    Q_D(QQuickAnchors);
    d->topMarginExplicit = true;
    if (d->topMargin == offset)
        return;
    d->topMargin = offset;
    if (d->fill)
        d->fillChanged();
    else
        d->updateVerticalAnchors();
    emit topMarginChanged();
}

void QQuickAnchors::resetTopMargin()
{
    Q_D(QQuickAnchors);
    d->topMarginExplicit = false;
    if (d->topMargin == d->margins)
        return;
    d->topMargin = d->margins;
    if (d->fill)
        d->fillChanged();
    else
        d->updateVerticalAnchors();
    emit topMarginChanged();
}

qreal QQuickAnchors::bottomMargin() const
{
    Q_D(const QQuickAnchors);
    return d->bottomMargin;
}

void QQuickAnchors::setBottomMargin(qreal offset)
{
    Q_D(QQuickAnchors);
    d->bottomMarginExplicit = true;
    if (d->bottomMargin == offset)
        return;
    d->bottomMargin = offset;
    if (d->fill)
        d->fillChanged();
    else
        d->updateVerticalAnchors();
    emit bottomMarginChanged();
}

void QQuickAnchors::resetBottomMargin()
{
    Q_D(QQuickAnchors);
    d->bottomMarginExplicit = false;
    if (d->bottomMargin == d->margins)
        return;
    d->bottomMargin = d->margins;
    if (d->fill)
        d->fillChanged();
    else
        d->updateVerticalAnchors();
    emit bottomMarginChanged();
}

qreal QQuickAnchors::verticalCenterOffset() const
{
    Q_D(const QQuickAnchors);
    return d->vCenterOffset;
}

void QQuickAnchors::setVerticalCenterOffset(qreal offset)
{
    Q_D(QQuickAnchors);
    if (d->vCenterOffset == offset)
        return;
    d->vCenterOffset = offset;
    if (d->centerIn)
        d->centerInChanged();
    else
        d->updateVerticalAnchors();
    emit verticalCenterOffsetChanged();
}

qreal QQuickAnchors::baselineOffset() const
{
    Q_D(const QQuickAnchors);
    return d->baselineOffset;
}

void QQuickAnchors::setBaselineOffset(qreal offset)
{
    Q_D(QQuickAnchors);
    if (d->baselineOffset == offset)
        return;
    d->baselineOffset = offset;
    d->updateVerticalAnchors();
    emit baselineOffsetChanged();
}

QQuickAnchors::Anchors QQuickAnchors::usedAnchors() const
{
    Q_D(const QQuickAnchors);
    return static_cast<QQuickAnchors::Anchors>(d->usedAnchors);
}

bool QQuickAnchorsPrivate::checkHValid() const
{
    if (usedAnchors & QQuickAnchors::LeftAnchor &&
        usedAnchors & QQuickAnchors::RightAnchor &&
        usedAnchors & QQuickAnchors::HCenterAnchor) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot specify left, right, and horizontalCenter anchors at the same time.");
        return false;
    }

    return true;
}

bool QQuickAnchorsPrivate::checkHAnchorValid(QQuickAnchorLine anchor) const
{
    if (!anchor.item) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor to a null item.");
        return false;
    } else if (anchor.anchorLine & QQuickAnchors::Vertical_Mask) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor a horizontal edge to a vertical edge.");
        return false;
    } else if (anchor.item != readParentItem(item)
               && readParentItem(anchor.item) != readParentItem(item)) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor to an item that isn't a parent or sibling.");
        return false;
    } else if (anchor.item == item) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor item to self.");
        return false;
    }

    return true;
}

bool QQuickAnchorsPrivate::checkVValid() const
{
    if (usedAnchors & QQuickAnchors::TopAnchor &&
        usedAnchors & QQuickAnchors::BottomAnchor &&
        usedAnchors & QQuickAnchors::VCenterAnchor) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot specify top, bottom, and verticalCenter anchors at the same time.");
        return false;
    } else if (usedAnchors & QQuickAnchors::BaselineAnchor &&
               (usedAnchors & QQuickAnchors::TopAnchor ||
                usedAnchors & QQuickAnchors::BottomAnchor ||
                usedAnchors & QQuickAnchors::VCenterAnchor)) {
        qmlInfo(item) << QQuickAnchors::tr("Baseline anchor cannot be used in conjunction with top, bottom, or verticalCenter anchors.");
        return false;
    }

    return true;
}

bool QQuickAnchorsPrivate::checkVAnchorValid(QQuickAnchorLine anchor) const
{
    if (!anchor.item) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor to a null item.");
        return false;
    } else if (anchor.anchorLine & QQuickAnchors::Horizontal_Mask) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor a vertical edge to a horizontal edge.");
        return false;
    } else if (anchor.item != readParentItem(item)
               && readParentItem(anchor.item) != readParentItem(item)) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor to an item that isn't a parent or sibling.");
        return false;
    } else if (anchor.item == item){
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor item to self.");
        return false;
    }

    return true;
}

QT_END_NAMESPACE

#include <moc_qquickanchors_p.cpp>

