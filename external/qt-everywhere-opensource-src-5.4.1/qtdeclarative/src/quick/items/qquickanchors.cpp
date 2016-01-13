/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickanchors_p_p.h"

#include "qquickitem.h"
#include "qquickitem_p.h"

#include <qqmlinfo.h>

QT_BEGIN_NAMESPACE

//TODO: should we cache relationships, so we don't have to check each time (parent-child or sibling)?
//TODO: support non-parent, non-sibling (need to find lowest common ancestor)

static inline qreal hcenter(QQuickItem *item)
{
    qreal width = item->width();
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

static inline qreal vcenter(QQuickItem *item)
{
    qreal height = item->height();
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

//### const item?
//local position
static qreal position(QQuickItem *item, QQuickAnchorLine::AnchorLine anchorLine)
{
    qreal ret = 0.0;
    switch (anchorLine) {
    case QQuickAnchorLine::Left:
        ret = item->x();
        break;
    case QQuickAnchorLine::Right:
        ret = item->x() + item->width();
        break;
    case QQuickAnchorLine::Top:
        ret = item->y();
        break;
    case QQuickAnchorLine::Bottom:
        ret = item->y() + item->height();
        break;
    case QQuickAnchorLine::HCenter:
        ret = item->x() + hcenter(item);
        break;
    case QQuickAnchorLine::VCenter:
        ret = item->y() + vcenter(item);
        break;
    case QQuickAnchorLine::Baseline:
        ret = item->y() + item->baselineOffset();
        break;
    default:
        break;
    }

    return ret;
}

//position when origin is 0,0
static qreal adjustedPosition(QQuickItem *item, QQuickAnchorLine::AnchorLine anchorLine)
{
    qreal ret = 0.0;
    switch (anchorLine) {
    case QQuickAnchorLine::Left:
        ret = 0.0;
        break;
    case QQuickAnchorLine::Right:
        ret = item->width();
        break;
    case QQuickAnchorLine::Top:
        ret = 0.0;
        break;
    case QQuickAnchorLine::Bottom:
        ret = item->height();
        break;
    case QQuickAnchorLine::HCenter:
        ret = hcenter(item);
        break;
    case QQuickAnchorLine::VCenter:
        ret = vcenter(item);
        break;
    case QQuickAnchorLine::Baseline:
        ret = item->baselineOffset();
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
    d->remDepend(d->left.item);
    d->remDepend(d->right.item);
    d->remDepend(d->top.item);
    d->remDepend(d->bottom.item);
    d->remDepend(d->vCenter.item);
    d->remDepend(d->hCenter.item);
    d->remDepend(d->baseline.item);
}

void QQuickAnchorsPrivate::fillChanged()
{
    Q_Q(QQuickAnchors);
    if (!fill || !isItemComplete())
        return;

    if (updatingFill < 2) {
        ++updatingFill;

        qreal horizontalMargin = q->mirrored() ? rightMargin : leftMargin;

        if (fill == item->parentItem()) {                         //child-parent
            setItemPos(QPointF(horizontalMargin, topMargin));
        } else if (fill->parentItem() == item->parentItem()) {   //siblings
            setItemPos(QPointF(fill->x()+horizontalMargin, fill->y()+topMargin));
        }
        setItemSize(QSizeF(fill->width()-leftMargin-rightMargin, fill->height()-topMargin-bottomMargin));

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
        if (centerIn == item->parentItem()) {
            QPointF p(hcenter(item->parentItem()) - hcenter(item) + effectiveHCenterOffset,
                      vcenter(item->parentItem()) - vcenter(item) + vCenterOffset);
            setItemPos(p);

        } else if (centerIn->parentItem() == item->parentItem()) {
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
    if (left.item == item) {
        left.item = 0;
        usedAnchors &= ~QQuickAnchors::LeftAnchor;
    }
    if (right.item == item) {
        right.item = 0;
        usedAnchors &= ~QQuickAnchors::RightAnchor;
    }
    if (top.item == item) {
        top.item = 0;
        usedAnchors &= ~QQuickAnchors::TopAnchor;
    }
    if (bottom.item == item) {
        bottom.item = 0;
        usedAnchors &= ~QQuickAnchors::BottomAnchor;
    }
    if (vCenter.item == item) {
        vCenter.item = 0;
        usedAnchors &= ~QQuickAnchors::VCenterAnchor;
    }
    if (hCenter.item == item) {
        hCenter.item = 0;
        usedAnchors &= ~QQuickAnchors::HCenterAnchor;
    }
    if (baseline.item == item) {
        baseline.item = 0;
        usedAnchors &= ~QQuickAnchors::BaselineAnchor;
    }
}

int QQuickAnchorsPrivate::calculateDependency(QQuickItem *controlItem)
{
    QQuickItemPrivate::GeometryChangeTypes dependency = QQuickItemPrivate::NoChange;

    if (!controlItem || inDestructor)
        return dependency;

    if (fill == controlItem) {
        if (controlItem == item->parentItem())
            dependency |= QQuickItemPrivate::SizeChange;
        else    //sibling
            dependency |= QQuickItemPrivate::GeometryChange;
        return dependency;  //exit early
    }

    if (centerIn == controlItem) {
        if (controlItem == item->parentItem())
            dependency |= QQuickItemPrivate::SizeChange;
        else    //sibling
            dependency |= QQuickItemPrivate::GeometryChange;
        return dependency;  //exit early
    }

    if ((usedAnchors & QQuickAnchors::LeftAnchor && left.item == controlItem) ||
        (usedAnchors & QQuickAnchors::RightAnchor && right.item == controlItem) ||
        (usedAnchors & QQuickAnchors::HCenterAnchor && hCenter.item == controlItem)) {
        if (controlItem == item->parentItem())
            dependency |= QQuickItemPrivate::WidthChange;
        else    //sibling
            dependency |= QFlags<QQuickItemPrivate::GeometryChangeType>(QQuickItemPrivate::XChange | QQuickItemPrivate::WidthChange);
    }

    if ((usedAnchors & QQuickAnchors::TopAnchor && top.item == controlItem) ||
        (usedAnchors & QQuickAnchors::BottomAnchor && bottom.item == controlItem) ||
        (usedAnchors & QQuickAnchors::VCenterAnchor && vCenter.item == controlItem) ||
        (usedAnchors & QQuickAnchors::BaselineAnchor && baseline.item == controlItem)) {
        if (controlItem == item->parentItem())
            dependency |= QQuickItemPrivate::HeightChange;
        else    //sibling
            dependency |= QFlags<QQuickItemPrivate::GeometryChangeType>(QQuickItemPrivate::YChange | QQuickItemPrivate::HeightChange);
    }

    return dependency;
}

void QQuickAnchorsPrivate::addDepend(QQuickItem *item)
{
    if (!item || !componentComplete)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    p->updateOrAddGeometryChangeListener(this, QFlags<QQuickItemPrivate::GeometryChangeType>(calculateDependency(item)));
}

void QQuickAnchorsPrivate::remDepend(QQuickItem *item)
{
    if (!item || !componentComplete)
        return;

    QQuickItemPrivate *p = QQuickItemPrivate::get(item);
    p->updateOrRemoveGeometryChangeListener(this, QFlags<QQuickItemPrivate::GeometryChangeType>(calculateDependency(item)));
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
    QSet<QQuickItem *> dependencies;
    dependencies << fill << centerIn
                 << left.item << right.item << hCenter.item
                 << top.item << bottom.item << vCenter.item << baseline.item;

    foreach (QQuickItem *dependency, dependencies)
        addDepend(dependency);

    update();
}


void QQuickAnchorsPrivate::update()
{
    fillChanged();
    centerInChanged();
    if (usedAnchors & QQuickAnchorLine::Horizontal_Mask)
        updateHorizontalAnchors();
    if (usedAnchors & QQuickAnchorLine::Vertical_Mask)
        updateVerticalAnchors();
}

void QQuickAnchorsPrivate::itemGeometryChanged(QQuickItem *, const QRectF &newG, const QRectF &oldG)
{
    fillChanged();
    centerInChanged();
    if ((usedAnchors & QQuickAnchorLine::Horizontal_Mask) && (newG.x() != oldG.x() || newG.width() != oldG.width()))
        updateHorizontalAnchors();
    if ((usedAnchors & QQuickAnchorLine::Vertical_Mask) && (newG.y() != oldG.y() || newG.height() != oldG.height()))
        updateVerticalAnchors();
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
    if (f != d->item->parentItem() && f->parentItem() != d->item->parentItem()){
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
    if (c != d->item->parentItem() && c->parentItem() != d->item->parentItem()){
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

bool QQuickAnchorsPrivate::calcStretch(const QQuickAnchorLine &edge1,
                                    const QQuickAnchorLine &edge2,
                                    qreal offset1,
                                    qreal offset2,
                                    QQuickAnchorLine::AnchorLine line,
                                    qreal &stretch)
{
    bool edge1IsParent = (edge1.item == item->parentItem());
    bool edge2IsParent = (edge2.item == item->parentItem());
    bool edge1IsSibling = (edge1.item->parentItem() == item->parentItem());
    bool edge2IsSibling = (edge2.item->parentItem() == item->parentItem());

    bool invalid = false;
    if ((edge2IsParent && edge1IsParent) || (edge2IsSibling && edge1IsSibling)) {
        stretch = (position(edge2.item, edge2.anchorLine) + offset2)
                    - (position(edge1.item, edge1.anchorLine) + offset1);
    } else if (edge2IsParent && edge1IsSibling) {
        stretch = (position(edge2.item, edge2.anchorLine) + offset2)
                    - (position(item->parentItem(), line)
                    + position(edge1.item, edge1.anchorLine) + offset1);
    } else if (edge2IsSibling && edge1IsParent) {
        stretch = (position(item->parentItem(), line) + position(edge2.item, edge2.anchorLine) + offset2)
                    - (position(edge1.item, edge1.anchorLine) + offset1);
    } else
        invalid = true;

    return invalid;
}

void QQuickAnchorsPrivate::updateVerticalAnchors()
{
    if (fill || centerIn || !isItemComplete())
        return;

    if (updatingVerticalAnchor < 2) {
        ++updatingVerticalAnchor;
        if (usedAnchors & QQuickAnchors::TopAnchor) {
            //Handle stretching
            bool invalid = true;
            qreal height = 0.0;
            if (usedAnchors & QQuickAnchors::BottomAnchor) {
                invalid = calcStretch(top, bottom, topMargin, -bottomMargin, QQuickAnchorLine::Top, height);
            } else if (usedAnchors & QQuickAnchors::VCenterAnchor) {
                invalid = calcStretch(top, vCenter, topMargin, vCenterOffset, QQuickAnchorLine::Top, height);
                height *= 2;
            }
            if (!invalid)
                setItemHeight(height);

            //Handle top
            if (top.item == item->parentItem()) {
                setItemY(adjustedPosition(top.item, top.anchorLine) + topMargin);
            } else if (top.item->parentItem() == item->parentItem()) {
                setItemY(position(top.item, top.anchorLine) + topMargin);
            }
        } else if (usedAnchors & QQuickAnchors::BottomAnchor) {
            //Handle stretching (top + bottom case is handled above)
            if (usedAnchors & QQuickAnchors::VCenterAnchor) {
                qreal height = 0.0;
                bool invalid = calcStretch(vCenter, bottom, vCenterOffset, -bottomMargin,
                                              QQuickAnchorLine::Top, height);
                if (!invalid)
                    setItemHeight(height*2);
            }

            //Handle bottom
            if (bottom.item == item->parentItem()) {
                setItemY(adjustedPosition(bottom.item, bottom.anchorLine) - item->height() - bottomMargin);
            } else if (bottom.item->parentItem() == item->parentItem()) {
                setItemY(position(bottom.item, bottom.anchorLine) - item->height() - bottomMargin);
            }
        } else if (usedAnchors & QQuickAnchors::VCenterAnchor) {
            //(stetching handled above)

            //Handle vCenter
            if (vCenter.item == item->parentItem()) {
                setItemY(adjustedPosition(vCenter.item, vCenter.anchorLine)
                              - vcenter(item) + vCenterOffset);
            } else if (vCenter.item->parentItem() == item->parentItem()) {
                setItemY(position(vCenter.item, vCenter.anchorLine) - vcenter(item) + vCenterOffset);
            }
        } else if (usedAnchors & QQuickAnchors::BaselineAnchor) {
            //Handle baseline
            if (baseline.item == item->parentItem()) {
                setItemY(adjustedPosition(baseline.item, baseline.anchorLine) - item->baselineOffset() + baselineOffset);
            } else if (baseline.item->parentItem() == item->parentItem()) {
                setItemY(position(baseline.item, baseline.anchorLine) - item->baselineOffset() + baselineOffset);
            }
        }
        --updatingVerticalAnchor;
    } else {
        // ### Make this certain :)
        qmlInfo(item) << QQuickAnchors::tr("Possible anchor loop detected on vertical anchor.");
    }
}

inline QQuickAnchorLine::AnchorLine reverseAnchorLine(QQuickAnchorLine::AnchorLine anchorLine)
{
    if (anchorLine == QQuickAnchorLine::Left) {
        return QQuickAnchorLine::Right;
    } else if (anchorLine == QQuickAnchorLine::Right) {
        return QQuickAnchorLine::Left;
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
        QQuickAnchorLine effectiveLeft, effectiveRight, effectiveHorizontalCenter;
        QQuickAnchors::Anchor effectiveLeftAnchor, effectiveRightAnchor;
        if (q->mirrored()) {
            effectiveLeftAnchor = QQuickAnchors::RightAnchor;
            effectiveRightAnchor = QQuickAnchors::LeftAnchor;
            effectiveLeft.item = right.item;
            effectiveLeft.anchorLine = reverseAnchorLine(right.anchorLine);
            effectiveRight.item = left.item;
            effectiveRight.anchorLine = reverseAnchorLine(left.anchorLine);
            effectiveHorizontalCenter.item = hCenter.item;
            effectiveHorizontalCenter.anchorLine = reverseAnchorLine(hCenter.anchorLine);
            effectiveLeftMargin = rightMargin;
            effectiveRightMargin = leftMargin;
            effectiveHorizontalCenterOffset = -hCenterOffset;
        } else {
            effectiveLeftAnchor = QQuickAnchors::LeftAnchor;
            effectiveRightAnchor = QQuickAnchors::RightAnchor;
            effectiveLeft = left;
            effectiveRight = right;
            effectiveHorizontalCenter = hCenter;
            effectiveLeftMargin = leftMargin;
            effectiveRightMargin = rightMargin;
            effectiveHorizontalCenterOffset = hCenterOffset;
        }

        if (usedAnchors & effectiveLeftAnchor) {
            //Handle stretching
            bool invalid = true;
            qreal width = 0.0;
            if (usedAnchors & effectiveRightAnchor) {
                invalid = calcStretch(effectiveLeft, effectiveRight, effectiveLeftMargin, -effectiveRightMargin, QQuickAnchorLine::Left, width);
            } else if (usedAnchors & QQuickAnchors::HCenterAnchor) {
                invalid = calcStretch(effectiveLeft, effectiveHorizontalCenter, effectiveLeftMargin, effectiveHorizontalCenterOffset, QQuickAnchorLine::Left, width);
                width *= 2;
            }
            if (!invalid)
                setItemWidth(width);

            //Handle left
            if (effectiveLeft.item == item->parentItem()) {
                setItemX(adjustedPosition(effectiveLeft.item, effectiveLeft.anchorLine) + effectiveLeftMargin);
            } else if (effectiveLeft.item->parentItem() == item->parentItem()) {
                setItemX(position(effectiveLeft.item, effectiveLeft.anchorLine) + effectiveLeftMargin);
            }
        } else if (usedAnchors & effectiveRightAnchor) {
            //Handle stretching (left + right case is handled in updateLeftAnchor)
            if (usedAnchors & QQuickAnchors::HCenterAnchor) {
                qreal width = 0.0;
                bool invalid = calcStretch(effectiveHorizontalCenter, effectiveRight, effectiveHorizontalCenterOffset, -effectiveRightMargin,
                                              QQuickAnchorLine::Left, width);
                if (!invalid)
                    setItemWidth(width*2);
            }

            //Handle right
            if (effectiveRight.item == item->parentItem()) {
                setItemX(adjustedPosition(effectiveRight.item, effectiveRight.anchorLine) - item->width() - effectiveRightMargin);
            } else if (effectiveRight.item->parentItem() == item->parentItem()) {
                setItemX(position(effectiveRight.item, effectiveRight.anchorLine) - item->width() - effectiveRightMargin);
            }
        } else if (usedAnchors & QQuickAnchors::HCenterAnchor) {
            //Handle hCenter
            if (effectiveHorizontalCenter.item == item->parentItem()) {
                setItemX(adjustedPosition(effectiveHorizontalCenter.item, effectiveHorizontalCenter.anchorLine) - hcenter(item) + effectiveHorizontalCenterOffset);
            } else if (effectiveHorizontalCenter.item->parentItem() == item->parentItem()) {
                setItemX(position(effectiveHorizontalCenter.item, effectiveHorizontalCenter.anchorLine) - hcenter(item) + effectiveHorizontalCenterOffset);
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
    return d->top;
}

void QQuickAnchors::setTop(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) || d->top == edge)
        return;

    d->usedAnchors |= TopAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~TopAnchor;
        return;
    }

    QQuickItem *oldTop = d->top.item;
    d->top = edge;
    d->remDepend(oldTop);
    d->addDepend(d->top.item);
    emit topChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetTop()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~TopAnchor;
    d->remDepend(d->top.item);
    d->top = QQuickAnchorLine();
    emit topChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::bottom() const
{
    Q_D(const QQuickAnchors);
    return d->bottom;
}

void QQuickAnchors::setBottom(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) || d->bottom == edge)
        return;

    d->usedAnchors |= BottomAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~BottomAnchor;
        return;
    }

    QQuickItem *oldBottom = d->bottom.item;
    d->bottom = edge;
    d->remDepend(oldBottom);
    d->addDepend(d->bottom.item);
    emit bottomChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetBottom()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~BottomAnchor;
    d->remDepend(d->bottom.item);
    d->bottom = QQuickAnchorLine();
    emit bottomChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::verticalCenter() const
{
    Q_D(const QQuickAnchors);
    return d->vCenter;
}

void QQuickAnchors::setVerticalCenter(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) || d->vCenter == edge)
        return;

    d->usedAnchors |= VCenterAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~VCenterAnchor;
        return;
    }

    QQuickItem *oldVCenter = d->vCenter.item;
    d->vCenter = edge;
    d->remDepend(oldVCenter);
    d->addDepend(d->vCenter.item);
    emit verticalCenterChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetVerticalCenter()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~VCenterAnchor;
    d->remDepend(d->vCenter.item);
    d->vCenter = QQuickAnchorLine();
    emit verticalCenterChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::baseline() const
{
    Q_D(const QQuickAnchors);
    return d->baseline;
}

void QQuickAnchors::setBaseline(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkVAnchorValid(edge) || d->baseline == edge)
        return;

    d->usedAnchors |= BaselineAnchor;

    if (!d->checkVValid()) {
        d->usedAnchors &= ~BaselineAnchor;
        return;
    }

    QQuickItem *oldBaseline = d->baseline.item;
    d->baseline = edge;
    d->remDepend(oldBaseline);
    d->addDepend(d->baseline.item);
    emit baselineChanged();
    d->updateVerticalAnchors();
}

void QQuickAnchors::resetBaseline()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~BaselineAnchor;
    d->remDepend(d->baseline.item);
    d->baseline = QQuickAnchorLine();
    emit baselineChanged();
    d->updateVerticalAnchors();
}

QQuickAnchorLine QQuickAnchors::left() const
{
    Q_D(const QQuickAnchors);
    return d->left;
}

void QQuickAnchors::setLeft(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) || d->left == edge)
        return;

    d->usedAnchors |= LeftAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~LeftAnchor;
        return;
    }

    QQuickItem *oldLeft = d->left.item;
    d->left = edge;
    d->remDepend(oldLeft);
    d->addDepend(d->left.item);
    emit leftChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetLeft()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~LeftAnchor;
    d->remDepend(d->left.item);
    d->left = QQuickAnchorLine();
    emit leftChanged();
    d->updateHorizontalAnchors();
}

QQuickAnchorLine QQuickAnchors::right() const
{
    Q_D(const QQuickAnchors);
    return d->right;
}

void QQuickAnchors::setRight(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) || d->right == edge)
        return;

    d->usedAnchors |= RightAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~RightAnchor;
        return;
    }

    QQuickItem *oldRight = d->right.item;
    d->right = edge;
    d->remDepend(oldRight);
    d->addDepend(d->right.item);
    emit rightChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetRight()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~RightAnchor;
    d->remDepend(d->right.item);
    d->right = QQuickAnchorLine();
    emit rightChanged();
    d->updateHorizontalAnchors();
}

QQuickAnchorLine QQuickAnchors::horizontalCenter() const
{
    Q_D(const QQuickAnchors);
    return d->hCenter;
}

void QQuickAnchors::setHorizontalCenter(const QQuickAnchorLine &edge)
{
    Q_D(QQuickAnchors);
    if (!d->checkHAnchorValid(edge) || d->hCenter == edge)
        return;

    d->usedAnchors |= HCenterAnchor;

    if (!d->checkHValid()) {
        d->usedAnchors &= ~HCenterAnchor;
        return;
    }

    QQuickItem *oldHCenter = d->hCenter.item;
    d->hCenter = edge;
    d->remDepend(oldHCenter);
    d->addDepend(d->hCenter.item);
    emit horizontalCenterChanged();
    d->updateHorizontalAnchors();
}

void QQuickAnchors::resetHorizontalCenter()
{
    Q_D(QQuickAnchors);
    d->usedAnchors &= ~HCenterAnchor;
    d->remDepend(d->hCenter.item);
    d->hCenter = QQuickAnchorLine();
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
    return d->usedAnchors;
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
    } else if (anchor.anchorLine & QQuickAnchorLine::Vertical_Mask) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor a horizontal edge to a vertical edge.");
        return false;
    } else if (anchor.item != item->parentItem() && anchor.item->parentItem() != item->parentItem()){
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
    } else if (anchor.anchorLine & QQuickAnchorLine::Horizontal_Mask) {
        qmlInfo(item) << QQuickAnchors::tr("Cannot anchor a vertical edge to a horizontal edge.");
        return false;
    } else if (anchor.item != item->parentItem() && anchor.item->parentItem() != item->parentItem()){
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

