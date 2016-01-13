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

#include "qquickstateoperations_p.h"
#include "qquickitem_p.h"

#include <private/qquickstate_p_p.h>

#include <QtQml/qqmlinfo.h>
#include <QtCore/qmath.h>

QT_BEGIN_NAMESPACE

class QQuickParentChangePrivate : public QQuickStateOperationPrivate
{
    Q_DECLARE_PUBLIC(QQuickParentChange)
public:
    QQuickParentChangePrivate() : target(0), parent(0), origParent(0), origStackBefore(0),
        rewindParent(0), rewindStackBefore(0) {}

    QQuickItem *target;
    QPointer<QQuickItem> parent;
    QPointer<QQuickItem> origParent;
    QPointer<QQuickItem> origStackBefore;
    QQuickItem *rewindParent;
    QQuickItem *rewindStackBefore;

    QQmlNullableValue<QQmlScriptString> xString;
    QQmlNullableValue<QQmlScriptString> yString;
    QQmlNullableValue<QQmlScriptString> widthString;
    QQmlNullableValue<QQmlScriptString> heightString;
    QQmlNullableValue<QQmlScriptString> scaleString;
    QQmlNullableValue<QQmlScriptString> rotationString;

    void doChange(QQuickItem *targetParent, QQuickItem *stackBefore = 0);
};

void QQuickParentChangePrivate::doChange(QQuickItem *targetParent, QQuickItem *stackBefore)
{
    if (targetParent && target && target->parentItem()) {
        Q_Q(QQuickParentChange);
        bool ok;
        const QTransform &transform = target->parentItem()->itemTransform(targetParent, &ok);
        if (transform.type() >= QTransform::TxShear || !ok) {
            qmlInfo(q) << QQuickParentChange::tr("Unable to preserve appearance under complex transform");
            ok = false;
        }

        qreal scale = 1;
        qreal rotation = 0;
        bool isRotate = (transform.type() == QTransform::TxRotate) || (transform.m11() < 0);
        if (ok && !isRotate) {
            if (transform.m11() == transform.m22())
                scale = transform.m11();
            else {
                qmlInfo(q) << QQuickParentChange::tr("Unable to preserve appearance under non-uniform scale");
                ok = false;
            }
        } else if (ok && isRotate) {
            if (transform.m11() == transform.m22())
                scale = qSqrt(transform.m11()*transform.m11() + transform.m12()*transform.m12());
            else {
                qmlInfo(q) << QQuickParentChange::tr("Unable to preserve appearance under non-uniform scale");
                ok = false;
            }

            if (scale != 0)
                rotation = atan2(transform.m12()/scale, transform.m11()/scale) * 180/M_PI;
            else {
                qmlInfo(q) << QQuickParentChange::tr("Unable to preserve appearance under scale of 0");
                ok = false;
            }
        }

        const QPointF &point = transform.map(QPointF(target->x(),target->y()));
        qreal x = point.x();
        qreal y = point.y();

        // setParentItem will update the transformOriginPoint if needed
        target->setParentItem(targetParent);

        if (ok && target->transformOrigin() != QQuickItem::TopLeft) {
            qreal tempxt = target->transformOriginPoint().x();
            qreal tempyt = target->transformOriginPoint().y();
            QTransform t;
            t.translate(-tempxt, -tempyt);
            t.rotate(rotation);
            t.scale(scale, scale);
            t.translate(tempxt, tempyt);
            const QPointF &offset = t.map(QPointF(0,0));
            x += offset.x();
            y += offset.y();
        }

        if (ok) {
            //qDebug() << x << y << rotation << scale;
            target->setPosition(QPointF(x, y));
            target->setRotation(target->rotation() + rotation);
            target->setScale(target->scale() * scale);
        }
    } else if (target) {
        target->setParentItem(targetParent);
    }

    //restore the original stack position.
    //### if stackBefore has also been reparented this won't work
    if (stackBefore)
        target->stackBefore(stackBefore);
}

/*!
    \qmltype ParentChange
    \instantiates QQuickParentChange
    \inqmlmodule QtQuick
    \ingroup qtquick-states
    \brief Specifies how to reparent an Item in a state change

    ParentChange reparents an item while preserving its visual appearance (position, size,
    rotation, and scale) on screen. You can then specify a transition to move/resize/rotate/scale
    the item to its final intended appearance.

    ParentChange can only preserve visual appearance if no complex transforms are involved.
    More specifically, it will not work if the transform property has been set for any
    items involved in the reparenting (i.e. items in the common ancestor tree
    for the original and new parent).

    The example below displays a large red rectangle and a small blue rectangle, side by side.
    When the \c blueRect is clicked, it changes to the "reparented" state: its parent is changed to \c redRect and it is
    positioned at (10, 10) within the red rectangle, as specified in the ParentChange.

    \snippet qml/parentchange.qml 0

    \image parentchange.png

    You can specify at which point in a transition you want a ParentChange to occur by
    using a ParentAnimation.

    Note that unlike PropertyChanges, ParentChange expects an Item-based target; it will not work with
    arbitrary objects (for example, you couldn't use it to reparent a Timer).
*/

QQuickParentChange::QQuickParentChange(QObject *parent)
    : QQuickStateOperation(*(new QQuickParentChangePrivate), parent)
{
}

QQuickParentChange::~QQuickParentChange()
{
}

/*!
    \qmlproperty real QtQuick::ParentChange::x
    \qmlproperty real QtQuick::ParentChange::y
    \qmlproperty real QtQuick::ParentChange::width
    \qmlproperty real QtQuick::ParentChange::height
    \qmlproperty real QtQuick::ParentChange::scale
    \qmlproperty real QtQuick::ParentChange::rotation
    These properties hold the new position, size, scale, and rotation
    for the item in this state.
*/
QQmlScriptString QQuickParentChange::x() const
{
    Q_D(const QQuickParentChange);
    return d->xString.value;
}

void QQuickParentChange::setX(QQmlScriptString x)
{
    Q_D(QQuickParentChange);
    d->xString = x;
}

bool QQuickParentChange::xIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->xString.isValid();
}

QQmlScriptString QQuickParentChange::y() const
{
    Q_D(const QQuickParentChange);
    return d->yString.value;
}

void QQuickParentChange::setY(QQmlScriptString y)
{
    Q_D(QQuickParentChange);
    d->yString = y;
}

bool QQuickParentChange::yIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->yString.isValid();
}

QQmlScriptString QQuickParentChange::width() const
{
    Q_D(const QQuickParentChange);
    return d->widthString.value;
}

void QQuickParentChange::setWidth(QQmlScriptString width)
{
    Q_D(QQuickParentChange);
    d->widthString = width;
}

bool QQuickParentChange::widthIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->widthString.isValid();
}

QQmlScriptString QQuickParentChange::height() const
{
    Q_D(const QQuickParentChange);
    return d->heightString.value;
}

void QQuickParentChange::setHeight(QQmlScriptString height)
{
    Q_D(QQuickParentChange);
    d->heightString = height;
}

bool QQuickParentChange::heightIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->heightString.isValid();
}

QQmlScriptString QQuickParentChange::scale() const
{
    Q_D(const QQuickParentChange);
    return d->scaleString.value;
}

void QQuickParentChange::setScale(QQmlScriptString scale)
{
    Q_D(QQuickParentChange);
    d->scaleString = scale;
}

bool QQuickParentChange::scaleIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->scaleString.isValid();
}

QQmlScriptString QQuickParentChange::rotation() const
{
    Q_D(const QQuickParentChange);
    return d->rotationString.value;
}

void QQuickParentChange::setRotation(QQmlScriptString rotation)
{
    Q_D(QQuickParentChange);
    d->rotationString = rotation;
}

bool QQuickParentChange::rotationIsSet() const
{
    Q_D(const QQuickParentChange);
    return d->rotationString.isValid();
}

QQuickItem *QQuickParentChange::originalParent() const
{
    Q_D(const QQuickParentChange);
    return d->origParent;
}

/*!
    \qmlproperty Item QtQuick::ParentChange::target
    This property holds the item to be reparented
*/
QQuickItem *QQuickParentChange::object() const
{
    Q_D(const QQuickParentChange);
    return d->target;
}

void QQuickParentChange::setObject(QQuickItem *target)
{
    Q_D(QQuickParentChange);
    d->target = target;
}

/*!
    \qmlproperty Item QtQuick::ParentChange::parent
    This property holds the new parent for the item in this state.
*/
QQuickItem *QQuickParentChange::parent() const
{
    Q_D(const QQuickParentChange);
    return d->parent;
}

void QQuickParentChange::setParent(QQuickItem *parent)
{
    Q_D(QQuickParentChange);
    d->parent = parent;
}

QQuickStateOperation::ActionList QQuickParentChange::actions()
{
    Q_D(QQuickParentChange);
    if (!d->target || !d->parent)
        return ActionList();

    ActionList actions;

    QQuickStateAction a;
    a.event = this;
    actions << a;

    if (d->xString.isValid()) {
        bool ok = false;
        qreal x = d->xString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction xa(d->target, QLatin1String("x"), x);
            actions << xa;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->xString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("x"));
            newBinding->setTarget(property);
            QQuickStateAction xa;
            xa.property = property;
            xa.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            xa.fromValue = xa.property.read();
            xa.deletableToBinding = true;
            actions << xa;
        }
    }

    if (d->yString.isValid()) {
        bool ok = false;
        qreal y = d->yString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction ya(d->target, QLatin1String("y"), y);
            actions << ya;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->yString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("y"));
            newBinding->setTarget(property);
            QQuickStateAction ya;
            ya.property = property;
            ya.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            ya.fromValue = ya.property.read();
            ya.deletableToBinding = true;
            actions << ya;
        }
    }

    if (d->scaleString.isValid()) {
        bool ok = false;
        qreal scale = d->scaleString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction sa(d->target, QLatin1String("scale"), scale);
            actions << sa;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->scaleString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("scale"));
            newBinding->setTarget(property);
            QQuickStateAction sa;
            sa.property = property;
            sa.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            sa.fromValue = sa.property.read();
            sa.deletableToBinding = true;
            actions << sa;
        }
    }

    if (d->rotationString.isValid()) {
        bool ok = false;
        qreal rotation = d->rotationString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction ra(d->target, QLatin1String("rotation"), rotation);
            actions << ra;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->rotationString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("rotation"));
            newBinding->setTarget(property);
            QQuickStateAction ra;
            ra.property = property;
            ra.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            ra.fromValue = ra.property.read();
            ra.deletableToBinding = true;
            actions << ra;
        }
    }

    if (d->widthString.isValid()) {
        bool ok = false;
        qreal width = d->widthString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction wa(d->target, QLatin1String("width"), width);
            actions << wa;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->widthString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("width"));
            newBinding->setTarget(property);
            QQuickStateAction wa;
            wa.property = property;
            wa.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            wa.fromValue = wa.property.read();
            wa.deletableToBinding = true;
            actions << wa;
        }
    }

    if (d->heightString.isValid()) {
        bool ok = false;
        qreal height = d->heightString.value.numberLiteral(&ok);
        if (ok) {
            QQuickStateAction ha(d->target, QLatin1String("height"), height);
            actions << ha;
        } else {
            QQmlBinding *newBinding = new QQmlBinding(d->heightString.value, d->target, qmlContext(this));
            QQmlProperty property(d->target, QLatin1String("height"));
            newBinding->setTarget(property);
            QQuickStateAction ha;
            ha.property = property;
            ha.toBinding = QQmlAbstractBinding::getPointer(newBinding);
            ha.fromValue = ha.property.read();
            ha.deletableToBinding = true;
            actions << ha;
        }
    }

    return actions;
}

void QQuickParentChange::saveOriginals()
{
    Q_D(QQuickParentChange);
    saveCurrentValues();
    d->origParent = d->rewindParent;
    d->origStackBefore = d->rewindStackBefore;
}

/*void QQuickParentChange::copyOriginals(QQuickStateActionEvent *other)
{
    Q_D(QQuickParentChange);
    QQuickParentChange *pc = static_cast<QQuickParentChange*>(other);

    d->origParent = pc->d_func()->rewindParent;
    d->origStackBefore = pc->d_func()->rewindStackBefore;

    saveCurrentValues();
}*/

void QQuickParentChange::execute(Reason)
{
    Q_D(QQuickParentChange);
    d->doChange(d->parent);
}

bool QQuickParentChange::isReversable()
{
    return true;
}

void QQuickParentChange::reverse(Reason)
{
    Q_D(QQuickParentChange);
    d->doChange(d->origParent, d->origStackBefore);
}

QQuickStateActionEvent::EventType QQuickParentChange::type() const
{
    return ParentChange;
}

bool QQuickParentChange::override(QQuickStateActionEvent*other)
{
    Q_D(QQuickParentChange);
    if (other->type() != ParentChange)
        return false;
    if (QQuickParentChange *otherPC = static_cast<QQuickParentChange*>(other))
        return (d->target == otherPC->object());
    return false;
}

void QQuickParentChange::saveCurrentValues()
{
    Q_D(QQuickParentChange);
    if (!d->target) {
        d->rewindParent = 0;
        d->rewindStackBefore = 0;
        return;
    }

    d->rewindParent = d->target->parentItem();
    d->rewindStackBefore = 0;

    if (!d->rewindParent)
        return;

    QList<QQuickItem *> children = d->rewindParent->childItems();
    for (int ii = 0; ii < children.count() - 1; ++ii) {
        if (children.at(ii) == d->target) {
            d->rewindStackBefore = children.at(ii + 1);
            break;
        }
    }
}

void QQuickParentChange::rewind()
{
    Q_D(QQuickParentChange);
    d->doChange(d->rewindParent, d->rewindStackBefore);
}

/*!
    \qmltype AnchorChanges
    \instantiates QQuickAnchorChanges
    \inqmlmodule QtQuick
    \ingroup qtquick-states
    \brief Specifies how to change the anchors of an item in a state

    The AnchorChanges type is used to modify the anchors of an item in a \l State.

    AnchorChanges cannot be used to modify the margins on an item. For this, use
    PropertyChanges intead.

    In the following example we change the top and bottom anchors of an item
    using AnchorChanges, and the top and bottom anchor margins using
    PropertyChanges:

    \snippet qml/anchorchanges.qml 0

    \image anchorchanges.png

    AnchorChanges can be animated using AnchorAnimation.
    \qml
    //animate our anchor changes
    Transition {
        AnchorAnimation {}
    }
    \endqml

    Changes to anchor margins can be animated using NumberAnimation.

    For more information on anchors see \l {anchor-layout}{Anchor Layouts}.
*/

class QQuickAnchorSetPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickAnchorSet)
public:
    QQuickAnchorSetPrivate()
      : usedAnchors(0), resetAnchors(0)
    {
    }

    QQuickAnchors::Anchors usedAnchors;
    QQuickAnchors::Anchors resetAnchors;

    QQmlScriptString leftScript;
    QQmlScriptString rightScript;
    QQmlScriptString topScript;
    QQmlScriptString bottomScript;
    QQmlScriptString hCenterScript;
    QQmlScriptString vCenterScript;
    QQmlScriptString baselineScript;
};

QQuickAnchorSet::QQuickAnchorSet(QObject *parent)
  : QObject(*new QQuickAnchorSetPrivate, parent)
{
}

QQuickAnchorSet::~QQuickAnchorSet()
{
}

QQmlScriptString QQuickAnchorSet::top() const
{
    Q_D(const QQuickAnchorSet);
    return d->topScript;
}

void QQuickAnchorSet::setTop(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::TopAnchor;
    d->topScript = edge;
    if (edge.isUndefinedLiteral())
        resetTop();
}

void QQuickAnchorSet::resetTop()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::TopAnchor;
    d->resetAnchors |= QQuickAnchors::TopAnchor;
}

QQmlScriptString QQuickAnchorSet::bottom() const
{
    Q_D(const QQuickAnchorSet);
    return d->bottomScript;
}

void QQuickAnchorSet::setBottom(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::BottomAnchor;
    d->bottomScript = edge;
    if (edge.isUndefinedLiteral())
        resetBottom();
}

void QQuickAnchorSet::resetBottom()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::BottomAnchor;
    d->resetAnchors |= QQuickAnchors::BottomAnchor;
}

QQmlScriptString QQuickAnchorSet::verticalCenter() const
{
    Q_D(const QQuickAnchorSet);
    return d->vCenterScript;
}

void QQuickAnchorSet::setVerticalCenter(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::VCenterAnchor;
    d->vCenterScript = edge;
    if (edge.isUndefinedLiteral())
        resetVerticalCenter();
}

void QQuickAnchorSet::resetVerticalCenter()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::VCenterAnchor;
    d->resetAnchors |= QQuickAnchors::VCenterAnchor;
}

QQmlScriptString QQuickAnchorSet::baseline() const
{
    Q_D(const QQuickAnchorSet);
    return d->baselineScript;
}

void QQuickAnchorSet::setBaseline(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::BaselineAnchor;
    d->baselineScript = edge;
    if (edge.isUndefinedLiteral())
        resetBaseline();
}

void QQuickAnchorSet::resetBaseline()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::BaselineAnchor;
    d->resetAnchors |= QQuickAnchors::BaselineAnchor;
}

QQmlScriptString QQuickAnchorSet::left() const
{
    Q_D(const QQuickAnchorSet);
    return d->leftScript;
}

void QQuickAnchorSet::setLeft(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::LeftAnchor;
    d->leftScript = edge;
    if (edge.isUndefinedLiteral())
        resetLeft();
}

void QQuickAnchorSet::resetLeft()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::LeftAnchor;
    d->resetAnchors |= QQuickAnchors::LeftAnchor;
}

QQmlScriptString QQuickAnchorSet::right() const
{
    Q_D(const QQuickAnchorSet);
    return d->rightScript;
}

void QQuickAnchorSet::setRight(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::RightAnchor;
    d->rightScript = edge;
    if (edge.isUndefinedLiteral())
        resetRight();
}

void QQuickAnchorSet::resetRight()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::RightAnchor;
    d->resetAnchors |= QQuickAnchors::RightAnchor;
}

QQmlScriptString QQuickAnchorSet::horizontalCenter() const
{
    Q_D(const QQuickAnchorSet);
    return d->hCenterScript;
}

void QQuickAnchorSet::setHorizontalCenter(const QQmlScriptString &edge)
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors |= QQuickAnchors::HCenterAnchor;
    d->hCenterScript = edge;
    if (edge.isUndefinedLiteral())
        resetHorizontalCenter();
}

void QQuickAnchorSet::resetHorizontalCenter()
{
    Q_D(QQuickAnchorSet);
    d->usedAnchors &= ~QQuickAnchors::HCenterAnchor;
    d->resetAnchors |= QQuickAnchors::HCenterAnchor;
}

class QQuickAnchorChangesPrivate : public QQuickStateOperationPrivate
{
public:
    QQuickAnchorChangesPrivate()
        : target(0), anchorSet(new QQuickAnchorSet),
          leftBinding(0), rightBinding(0), hCenterBinding(0),
          topBinding(0), bottomBinding(0), vCenterBinding(0), baselineBinding(0),
          origLeftBinding(0), origRightBinding(0), origHCenterBinding(0),
          origTopBinding(0), origBottomBinding(0), origVCenterBinding(0),
          origBaselineBinding(0)
    {

    }
    ~QQuickAnchorChangesPrivate() { delete anchorSet; }

    QQuickItem *target;
    QQuickAnchorSet *anchorSet;

    QQmlBinding *leftBinding;
    QQmlBinding *rightBinding;
    QQmlBinding *hCenterBinding;
    QQmlBinding *topBinding;
    QQmlBinding *bottomBinding;
    QQmlBinding *vCenterBinding;
    QQmlBinding *baselineBinding;

    QQmlAbstractBinding *origLeftBinding;
    QQmlAbstractBinding *origRightBinding;
    QQmlAbstractBinding *origHCenterBinding;
    QQmlAbstractBinding *origTopBinding;
    QQmlAbstractBinding *origBottomBinding;
    QQmlAbstractBinding *origVCenterBinding;
    QQmlAbstractBinding *origBaselineBinding;

    QQuickAnchorLine rewindLeft;
    QQuickAnchorLine rewindRight;
    QQuickAnchorLine rewindHCenter;
    QQuickAnchorLine rewindTop;
    QQuickAnchorLine rewindBottom;
    QQuickAnchorLine rewindVCenter;
    QQuickAnchorLine rewindBaseline;

    qreal fromX;
    qreal fromY;
    qreal fromWidth;
    qreal fromHeight;

    qreal toX;
    qreal toY;
    qreal toWidth;
    qreal toHeight;

    qreal rewindX;
    qreal rewindY;
    qreal rewindWidth;
    qreal rewindHeight;

    bool applyOrigLeft;
    bool applyOrigRight;
    bool applyOrigHCenter;
    bool applyOrigTop;
    bool applyOrigBottom;
    bool applyOrigVCenter;
    bool applyOrigBaseline;

    QQmlNullableValue<qreal> origWidth;
    QQmlNullableValue<qreal> origHeight;
    qreal origX;
    qreal origY;

    QList<QQmlAbstractBinding*> oldBindings;

    QQmlProperty leftProp;
    QQmlProperty rightProp;
    QQmlProperty hCenterProp;
    QQmlProperty topProp;
    QQmlProperty bottomProp;
    QQmlProperty vCenterProp;
    QQmlProperty baselineProp;
};

QQuickAnchorChanges::QQuickAnchorChanges(QObject *parent)
 : QQuickStateOperation(*(new QQuickAnchorChangesPrivate), parent)
{
}

QQuickAnchorChanges::~QQuickAnchorChanges()
{
    /*
        if the anchorchanges is active at destruction, any non-active orig
        bindings need to be destroyed

        the basic logic is that if both e.g. left and origLeft are present,
        then we are active (otherwise left would have been destroyed), and
        left is in use and origLeft needs to be cleaned up.
    */
    Q_D(QQuickAnchorChanges);
    if (d->leftBinding && d->origLeftBinding)
        d->origLeftBinding->destroy();
    if (d->rightBinding && d->origRightBinding)
        d->origRightBinding->destroy();
    if (d->hCenterBinding && d->origHCenterBinding)
        d->origHCenterBinding->destroy();
    if (d->topBinding && d->origTopBinding)
        d->origTopBinding->destroy();
    if (d->bottomBinding && d->origBottomBinding)
        d->origBottomBinding->destroy();
    if (d->vCenterBinding && d->origVCenterBinding)
        d->origVCenterBinding->destroy();
    if (d->baselineBinding && d->origBaselineBinding)
        d->origBaselineBinding->destroy();
}

QQuickAnchorChanges::ActionList QQuickAnchorChanges::actions()
{
    Q_D(QQuickAnchorChanges);
    //### ASSERT these are all 0?
    d->leftBinding = d->rightBinding = d->hCenterBinding = d->topBinding
                   = d->bottomBinding = d->vCenterBinding = d->baselineBinding = 0;

    d->leftProp = QQmlProperty(d->target, QLatin1String("anchors.left"));
    d->rightProp = QQmlProperty(d->target, QLatin1String("anchors.right"));
    d->hCenterProp = QQmlProperty(d->target, QLatin1String("anchors.horizontalCenter"));
    d->topProp = QQmlProperty(d->target, QLatin1String("anchors.top"));
    d->bottomProp = QQmlProperty(d->target, QLatin1String("anchors.bottom"));
    d->vCenterProp = QQmlProperty(d->target, QLatin1String("anchors.verticalCenter"));
    d->baselineProp = QQmlProperty(d->target, QLatin1String("anchors.baseline"));

    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::LeftAnchor) {
        d->leftBinding = new QQmlBinding(d->anchorSet->d_func()->leftScript, d->target, qmlContext(this));
        d->leftBinding->setTarget(d->leftProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::RightAnchor) {
        d->rightBinding = new QQmlBinding(d->anchorSet->d_func()->rightScript, d->target, qmlContext(this));
        d->rightBinding->setTarget(d->rightProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::HCenterAnchor) {
        d->hCenterBinding = new QQmlBinding(d->anchorSet->d_func()->hCenterScript, d->target, qmlContext(this));
        d->hCenterBinding->setTarget(d->hCenterProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::TopAnchor) {
        d->topBinding = new QQmlBinding(d->anchorSet->d_func()->topScript, d->target, qmlContext(this));
        d->topBinding->setTarget(d->topProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::BottomAnchor) {
        d->bottomBinding = new QQmlBinding(d->anchorSet->d_func()->bottomScript, d->target, qmlContext(this));
        d->bottomBinding->setTarget(d->bottomProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::VCenterAnchor) {
        d->vCenterBinding = new QQmlBinding(d->anchorSet->d_func()->vCenterScript, d->target, qmlContext(this));
        d->vCenterBinding->setTarget(d->vCenterProp);
    }
    if (d->anchorSet->d_func()->usedAnchors & QQuickAnchors::BaselineAnchor) {
        d->baselineBinding = new QQmlBinding(d->anchorSet->d_func()->baselineScript, d->target, qmlContext(this));
        d->baselineBinding->setTarget(d->baselineProp);
    }

    QQuickStateAction a;
    a.event = this;
    return ActionList() << a;
}

QQuickAnchorSet *QQuickAnchorChanges::anchors()
{
    Q_D(QQuickAnchorChanges);
    return d->anchorSet;
}

/*!
    \qmlproperty Item QtQuick::AnchorChanges::target
    This property holds the \l Item for which the anchor changes will be applied.
*/
QQuickItem *QQuickAnchorChanges::object() const
{
    Q_D(const QQuickAnchorChanges);
    return d->target;
}

void QQuickAnchorChanges::setObject(QQuickItem *target)
{
    Q_D(QQuickAnchorChanges);
    d->target = target;
}

/*!
    \qmlpropertygroup QtQuick::AnchorChanges::anchors
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.left
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.right
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.horizontalCenter
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.top
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.bottom
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.verticalCenter
    \qmlproperty AnchorLine QtQuick::AnchorChanges::anchors.baseline

    These properties change the respective anchors of the item.

    To reset an anchor you can assign \c undefined:
    \qml
    AnchorChanges {
        target: myItem
        anchors.left: undefined          //remove myItem's left anchor
        anchors.right: otherItem.right
    }
    \endqml
*/

void QQuickAnchorChanges::execute(Reason reason)
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);
    //incorporate any needed "reverts"
    if (d->applyOrigLeft) {
        if (!d->origLeftBinding)
            targetPrivate->anchors()->resetLeft();
        QQmlPropertyPrivate::setBinding(d->leftProp, d->origLeftBinding);
    }
    if (d->applyOrigRight) {
        if (!d->origRightBinding)
            targetPrivate->anchors()->resetRight();
        QQmlPropertyPrivate::setBinding(d->rightProp, d->origRightBinding);
    }
    if (d->applyOrigHCenter) {
        if (!d->origHCenterBinding)
            targetPrivate->anchors()->resetHorizontalCenter();
        QQmlPropertyPrivate::setBinding(d->hCenterProp, d->origHCenterBinding);
    }
    if (d->applyOrigTop) {
        if (!d->origTopBinding)
            targetPrivate->anchors()->resetTop();
        QQmlPropertyPrivate::setBinding(d->topProp, d->origTopBinding);
    }
    if (d->applyOrigBottom) {
        if (!d->origBottomBinding)
            targetPrivate->anchors()->resetBottom();
        QQmlPropertyPrivate::setBinding(d->bottomProp, d->origBottomBinding);
    }
    if (d->applyOrigVCenter) {
        if (!d->origVCenterBinding)
            targetPrivate->anchors()->resetVerticalCenter();
        QQmlPropertyPrivate::setBinding(d->vCenterProp, d->origVCenterBinding);
    }
    if (d->applyOrigBaseline) {
        if (!d->origBaselineBinding)
            targetPrivate->anchors()->resetBaseline();
        QQmlPropertyPrivate::setBinding(d->baselineProp, d->origBaselineBinding);
    }

    //destroy old bindings
    if (reason == ActualChange) {
        for (int i = 0; i < d->oldBindings.size(); ++i) {
            QQmlAbstractBinding *binding = d->oldBindings.at(i);
            if (binding)
                binding->destroy();
        }
        d->oldBindings.clear();
    }

    //reset any anchors that have been specified as "undefined"
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::LeftAnchor) {
        targetPrivate->anchors()->resetLeft();
        QQmlPropertyPrivate::setBinding(d->leftProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::RightAnchor) {
        targetPrivate->anchors()->resetRight();
        QQmlPropertyPrivate::setBinding(d->rightProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::HCenterAnchor) {
        targetPrivate->anchors()->resetHorizontalCenter();
        QQmlPropertyPrivate::setBinding(d->hCenterProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::TopAnchor) {
        targetPrivate->anchors()->resetTop();
        QQmlPropertyPrivate::setBinding(d->topProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::BottomAnchor) {
        targetPrivate->anchors()->resetBottom();
        QQmlPropertyPrivate::setBinding(d->bottomProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::VCenterAnchor) {
        targetPrivate->anchors()->resetVerticalCenter();
        QQmlPropertyPrivate::setBinding(d->vCenterProp, 0);
    }
    if (d->anchorSet->d_func()->resetAnchors & QQuickAnchors::BaselineAnchor) {
        targetPrivate->anchors()->resetBaseline();
        QQmlPropertyPrivate::setBinding(d->baselineProp, 0);
    }

    //set any anchors that have been specified
    if (d->leftBinding)
        QQmlPropertyPrivate::setBinding(d->leftBinding->property(), d->leftBinding);
    if (d->rightBinding)
        QQmlPropertyPrivate::setBinding(d->rightBinding->property(), d->rightBinding);
    if (d->hCenterBinding)
        QQmlPropertyPrivate::setBinding(d->hCenterBinding->property(), d->hCenterBinding);
    if (d->topBinding)
        QQmlPropertyPrivate::setBinding(d->topBinding->property(), d->topBinding);
    if (d->bottomBinding)
        QQmlPropertyPrivate::setBinding(d->bottomBinding->property(), d->bottomBinding);
    if (d->vCenterBinding)
        QQmlPropertyPrivate::setBinding(d->vCenterBinding->property(), d->vCenterBinding);
    if (d->baselineBinding)
        QQmlPropertyPrivate::setBinding(d->baselineBinding->property(), d->baselineBinding);
}

bool QQuickAnchorChanges::isReversable()
{
    return true;
}

void QQuickAnchorChanges::reverse(Reason reason)
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);
    //reset any anchors set by the state
    if (d->leftBinding) {
        targetPrivate->anchors()->resetLeft();
        QQmlPropertyPrivate::setBinding(d->leftBinding->property(), 0);
        if (reason == ActualChange) {
            d->leftBinding->destroy(); d->leftBinding = 0;
        }
    }
    if (d->rightBinding) {
        targetPrivate->anchors()->resetRight();
        QQmlPropertyPrivate::setBinding(d->rightBinding->property(), 0);
        if (reason == ActualChange) {
            d->rightBinding->destroy(); d->rightBinding = 0;
        }
    }
    if (d->hCenterBinding) {
        targetPrivate->anchors()->resetHorizontalCenter();
        QQmlPropertyPrivate::setBinding(d->hCenterBinding->property(), 0);
        if (reason == ActualChange) {
            d->hCenterBinding->destroy(); d->hCenterBinding = 0;
        }
    }
    if (d->topBinding) {
        targetPrivate->anchors()->resetTop();
        QQmlPropertyPrivate::setBinding(d->topBinding->property(), 0);
        if (reason == ActualChange) {
            d->topBinding->destroy(); d->topBinding = 0;
        }
    }
    if (d->bottomBinding) {
        targetPrivate->anchors()->resetBottom();
        QQmlPropertyPrivate::setBinding(d->bottomBinding->property(), 0);
        if (reason == ActualChange) {
            d->bottomBinding->destroy(); d->bottomBinding = 0;
        }
    }
    if (d->vCenterBinding) {
        targetPrivate->anchors()->resetVerticalCenter();
        QQmlPropertyPrivate::setBinding(d->vCenterBinding->property(), 0);
        if (reason == ActualChange) {
            d->vCenterBinding->destroy(); d->vCenterBinding = 0;
        }
    }
    if (d->baselineBinding) {
        targetPrivate->anchors()->resetBaseline();
        QQmlPropertyPrivate::setBinding(d->baselineBinding->property(), 0);
        if (reason == ActualChange) {
            d->baselineBinding->destroy(); d->baselineBinding = 0;
        }
    }

    //restore previous anchors
    if (d->origLeftBinding)
        QQmlPropertyPrivate::setBinding(d->leftProp, d->origLeftBinding);
    if (d->origRightBinding)
        QQmlPropertyPrivate::setBinding(d->rightProp, d->origRightBinding);
    if (d->origHCenterBinding)
        QQmlPropertyPrivate::setBinding(d->hCenterProp, d->origHCenterBinding);
    if (d->origTopBinding)
        QQmlPropertyPrivate::setBinding(d->topProp, d->origTopBinding);
    if (d->origBottomBinding)
        QQmlPropertyPrivate::setBinding(d->bottomProp, d->origBottomBinding);
    if (d->origVCenterBinding)
        QQmlPropertyPrivate::setBinding(d->vCenterProp, d->origVCenterBinding);
    if (d->origBaselineBinding)
        QQmlPropertyPrivate::setBinding(d->baselineProp, d->origBaselineBinding);

    //restore any absolute geometry changed by the state's anchors
    QQuickAnchors::Anchors stateVAnchors = d->anchorSet->d_func()->usedAnchors & QQuickAnchors::Vertical_Mask;
    QQuickAnchors::Anchors origVAnchors = targetPrivate->anchors()->usedAnchors() & QQuickAnchors::Vertical_Mask;
    QQuickAnchors::Anchors stateHAnchors = d->anchorSet->d_func()->usedAnchors & QQuickAnchors::Horizontal_Mask;
    QQuickAnchors::Anchors origHAnchors = targetPrivate->anchors()->usedAnchors() & QQuickAnchors::Horizontal_Mask;

    bool stateSetWidth = (stateHAnchors &&
                          stateHAnchors != QQuickAnchors::LeftAnchor &&
                          stateHAnchors != QQuickAnchors::RightAnchor &&
                          stateHAnchors != QQuickAnchors::HCenterAnchor);
    bool origSetWidth = (origHAnchors &&
                         origHAnchors != QQuickAnchors::LeftAnchor &&
                         origHAnchors != QQuickAnchors::RightAnchor &&
                         origHAnchors != QQuickAnchors::HCenterAnchor);
    if (d->origWidth.isValid() && stateSetWidth && !origSetWidth)
        d->target->setWidth(d->origWidth.value);

    bool stateSetHeight = (stateVAnchors &&
                           stateVAnchors != QQuickAnchors::TopAnchor &&
                           stateVAnchors != QQuickAnchors::BottomAnchor &&
                           stateVAnchors != QQuickAnchors::VCenterAnchor &&
                           stateVAnchors != QQuickAnchors::BaselineAnchor);
    bool origSetHeight = (origVAnchors &&
                          origVAnchors != QQuickAnchors::TopAnchor &&
                          origVAnchors != QQuickAnchors::BottomAnchor &&
                          origVAnchors != QQuickAnchors::VCenterAnchor &&
                          origVAnchors != QQuickAnchors::BaselineAnchor);
    if (d->origHeight.isValid() && stateSetHeight && !origSetHeight)
        d->target->setHeight(d->origHeight.value);

    if (stateHAnchors && !origHAnchors)
        d->target->setX(d->origX);

    if (stateVAnchors && !origVAnchors)
        d->target->setY(d->origY);
}

QQuickStateActionEvent::EventType QQuickAnchorChanges::type() const
{
    return AnchorChanges;
}

QList<QQuickStateAction> QQuickAnchorChanges::additionalActions()
{
    Q_D(QQuickAnchorChanges);
    QList<QQuickStateAction> extra;

    QQuickAnchors::Anchors combined = d->anchorSet->d_func()->usedAnchors | d->anchorSet->d_func()->resetAnchors;
    bool hChange = combined & QQuickAnchors::Horizontal_Mask;
    bool vChange = combined & QQuickAnchors::Vertical_Mask;

    if (d->target) {
        QQuickStateAction a;
        if (hChange && d->fromX != d->toX) {
            a.property = QQmlProperty(d->target, QLatin1String("x"));
            a.toValue = d->toX;
            extra << a;
        }
        if (vChange && d->fromY != d->toY) {
            a.property = QQmlProperty(d->target, QLatin1String("y"));
            a.toValue = d->toY;
            extra << a;
        }
        if (hChange && d->fromWidth != d->toWidth) {
            a.property = QQmlProperty(d->target, QLatin1String("width"));
            a.toValue = d->toWidth;
            extra << a;
        }
        if (vChange && d->fromHeight != d->toHeight) {
            a.property = QQmlProperty(d->target, QLatin1String("height"));
            a.toValue = d->toHeight;
            extra << a;
        }
    }

    return extra;
}

bool QQuickAnchorChanges::changesBindings()
{
    return true;
}

void QQuickAnchorChanges::saveOriginals()
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    d->origLeftBinding = QQmlPropertyPrivate::binding(d->leftProp);
    d->origRightBinding = QQmlPropertyPrivate::binding(d->rightProp);
    d->origHCenterBinding = QQmlPropertyPrivate::binding(d->hCenterProp);
    d->origTopBinding = QQmlPropertyPrivate::binding(d->topProp);
    d->origBottomBinding = QQmlPropertyPrivate::binding(d->bottomProp);
    d->origVCenterBinding = QQmlPropertyPrivate::binding(d->vCenterProp);
    d->origBaselineBinding = QQmlPropertyPrivate::binding(d->baselineProp);

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);
    if (targetPrivate->widthValid)
        d->origWidth = d->target->width();
    if (targetPrivate->heightValid)
        d->origHeight = d->target->height();
    d->origX = d->target->x();
    d->origY = d->target->y();

    d->applyOrigLeft = d->applyOrigRight = d->applyOrigHCenter = d->applyOrigTop
      = d->applyOrigBottom = d->applyOrigVCenter = d->applyOrigBaseline = false;

    saveCurrentValues();
}

void QQuickAnchorChanges::copyOriginals(QQuickStateActionEvent *other)
{
    Q_D(QQuickAnchorChanges);
    QQuickAnchorChanges *ac = static_cast<QQuickAnchorChanges*>(other);
    QQuickAnchorChangesPrivate *acp = ac->d_func();

    QQuickAnchors::Anchors combined = acp->anchorSet->d_func()->usedAnchors |
                                            acp->anchorSet->d_func()->resetAnchors;

    //probably also need to revert some things
    d->applyOrigLeft = (combined & QQuickAnchors::LeftAnchor);
    d->applyOrigRight = (combined & QQuickAnchors::RightAnchor);
    d->applyOrigHCenter = (combined & QQuickAnchors::HCenterAnchor);
    d->applyOrigTop = (combined & QQuickAnchors::TopAnchor);
    d->applyOrigBottom = (combined & QQuickAnchors::BottomAnchor);
    d->applyOrigVCenter = (combined & QQuickAnchors::VCenterAnchor);
    d->applyOrigBaseline = (combined & QQuickAnchors::BaselineAnchor);

    d->origLeftBinding = acp->origLeftBinding;
    d->origRightBinding = acp->origRightBinding;
    d->origHCenterBinding = acp->origHCenterBinding;
    d->origTopBinding = acp->origTopBinding;
    d->origBottomBinding = acp->origBottomBinding;
    d->origVCenterBinding = acp->origVCenterBinding;
    d->origBaselineBinding = acp->origBaselineBinding;

    d->origWidth = acp->origWidth;
    d->origHeight = acp->origHeight;
    d->origX = acp->origX;
    d->origY = acp->origY;

    d->oldBindings.clear();
    d->oldBindings << acp->leftBinding << acp->rightBinding << acp->hCenterBinding
                << acp->topBinding << acp->bottomBinding << acp->vCenterBinding << acp->baselineBinding;

    //clear old values from other
    //### could this be generalized for all QQuickStateActionEvents, and called after copyOriginals?
    acp->leftBinding = 0;
    acp->rightBinding = 0;
    acp->hCenterBinding = 0;
    acp->topBinding = 0;
    acp->bottomBinding = 0;
    acp->vCenterBinding = 0;
    acp->baselineBinding = 0;
    acp->origLeftBinding = 0;
    acp->origRightBinding = 0;
    acp->origHCenterBinding = 0;
    acp->origTopBinding = 0;
    acp->origBottomBinding = 0;
    acp->origVCenterBinding = 0;
    acp->origBaselineBinding = 0;

    saveCurrentValues();
}

void QQuickAnchorChanges::clearBindings()
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    //### should this (saving "from" values) be moved to saveCurrentValues()?
    d->fromX = d->target->x();
    d->fromY = d->target->y();
    d->fromWidth = d->target->width();
    d->fromHeight = d->target->height();

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);
    //reset any anchors with corresponding reverts
    //reset any anchors that have been specified as "undefined"
    //reset any anchors that we'll be setting in the state
    QQuickAnchors::Anchors combined = d->anchorSet->d_func()->resetAnchors |
                                            d->anchorSet->d_func()->usedAnchors;
    if (d->applyOrigLeft || (combined & QQuickAnchors::LeftAnchor)) {
        targetPrivate->anchors()->resetLeft();
        QQmlPropertyPrivate::setBinding(d->leftProp, 0);
    }
    if (d->applyOrigRight || (combined & QQuickAnchors::RightAnchor)) {
        targetPrivate->anchors()->resetRight();
        QQmlPropertyPrivate::setBinding(d->rightProp, 0);
    }
    if (d->applyOrigHCenter || (combined & QQuickAnchors::HCenterAnchor)) {
        targetPrivate->anchors()->resetHorizontalCenter();
        QQmlPropertyPrivate::setBinding(d->hCenterProp, 0);
    }
    if (d->applyOrigTop || (combined & QQuickAnchors::TopAnchor)) {
        targetPrivate->anchors()->resetTop();
        QQmlPropertyPrivate::setBinding(d->topProp, 0);
    }
    if (d->applyOrigBottom || (combined & QQuickAnchors::BottomAnchor)) {
        targetPrivate->anchors()->resetBottom();
        QQmlPropertyPrivate::setBinding(d->bottomProp, 0);
    }
    if (d->applyOrigVCenter || (combined & QQuickAnchors::VCenterAnchor)) {
        targetPrivate->anchors()->resetVerticalCenter();
        QQmlPropertyPrivate::setBinding(d->vCenterProp, 0);
    }
    if (d->applyOrigBaseline || (combined & QQuickAnchors::BaselineAnchor)) {
        targetPrivate->anchors()->resetBaseline();
        QQmlPropertyPrivate::setBinding(d->baselineProp, 0);
    }
}

bool QQuickAnchorChanges::override(QQuickStateActionEvent*other)
{
    if (other->type() != AnchorChanges)
        return false;
    if (static_cast<QQuickStateActionEvent*>(this) == other)
        return true;
    if (static_cast<QQuickAnchorChanges*>(other)->object() == object())
        return true;
    return false;
}

void QQuickAnchorChanges::rewind()
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);

    //restore previous values (but not previous bindings, i.e. anchors)
    d->target->setX(d->rewindX);
    d->target->setY(d->rewindY);
    if (targetPrivate->widthValid) {
        d->target->setWidth(d->rewindWidth);
    }
    if (targetPrivate->heightValid) {
        d->target->setHeight(d->rewindHeight);
    }
}

void QQuickAnchorChanges::saveCurrentValues()
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    QQuickItemPrivate *targetPrivate = QQuickItemPrivate::get(d->target);
    d->rewindLeft = targetPrivate->anchors()->left();
    d->rewindRight = targetPrivate->anchors()->right();
    d->rewindHCenter = targetPrivate->anchors()->horizontalCenter();
    d->rewindTop = targetPrivate->anchors()->top();
    d->rewindBottom = targetPrivate->anchors()->bottom();
    d->rewindVCenter = targetPrivate->anchors()->verticalCenter();
    d->rewindBaseline = targetPrivate->anchors()->baseline();

    d->rewindX = d->target->x();
    d->rewindY = d->target->y();
    d->rewindWidth = d->target->width();
    d->rewindHeight = d->target->height();
}

void QQuickAnchorChanges::saveTargetValues()
{
    Q_D(QQuickAnchorChanges);
    if (!d->target)
        return;

    d->toX = d->target->x();
    d->toY = d->target->y();
    d->toWidth = d->target->width();
    d->toHeight = d->target->height();
}

#include <moc_qquickstateoperations_p.cpp>

QT_END_NAMESPACE

