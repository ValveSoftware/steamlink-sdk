/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickswipedelegate_p.h"
#include "qquickswipedelegate_p_p.h"
#include "qquickcontrol_p_p.h"
#include "qquickitemdelegate_p_p.h"
#include "qquickvelocitycalculator_p_p.h"

#include <QtGui/qstylehints.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype SwipeDelegate
    \inherits ItemDelegate
    \instantiates QQuickSwipeDelegate
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-delegates
    \brief Swipable item delegate.

    SwipeDelegate presents a view item that can be swiped left or right to
    expose more options or information. It is used as a delegate in views such
    as \l ListView.

    In the following example, SwipeDelegate is used in a \l ListView to allow
    items to be removed from it by swiping to the left:

    \snippet qtquickcontrols2-swipedelegate.qml 1

    SwipeDelegate inherits its API from \l ItemDelegate, which is inherited
    from AbstractButton. For instance, you can set \l {AbstractButton::text}{text},
    and react to \l {AbstractButton::clicked}{clicks} using the AbstractButton
    API.

    Information regarding the progress of a swipe, as well as the components
    that should be shown upon swiping, are both available through the
    \l {SwipeDelegate::}{swipe} grouped property object. For example,
    \c swipe.position holds the position of the
    swipe within the range \c -1.0 to \c 1.0. The \c swipe.left
    property determines which item will be displayed when the control is swiped
    to the right, and vice versa for \c swipe.right. The positioning of these
    components is left to applications to decide. For example, without specifying
    any position for \c swipe.left or \c swipe.right, the following will
    occur:

    \image qtquickcontrols2-swipedelegate.gif

    If \c swipe.left and \c swipe.right are anchored to the left and
    right of the \l {Control::}{background} item (respectively), they'll behave like this:

    \image qtquickcontrols2-swipedelegate-leading-trailing.gif

    When using \c swipe.left and \c swipe.right, the control cannot be
    swiped past the left and right edges. To achieve this type of "wrapping"
    behavior, set \c swipe.behind instead. This will result in the same
    item being shown regardless of which direction the control is swiped. For
    example, in the image below, we set \c swipe.behind and then swipe the
    control repeatedly in both directions:

    \image qtquickcontrols2-swipedelegate-behind.gif

    \sa {Customizing SwipeDelegate}, {Delegate Controls}
*/

namespace {
    typedef QQuickSwipeDelegateAttached Attached;

    Attached *attachedObject(QQuickItem *item) {
        return qobject_cast<Attached*>(qmlAttachedPropertiesObject<QQuickSwipeDelegate>(item, false));
    }

    enum PositionAnimation {
        DontAnimatePosition,
        AnimatePosition
    };
}

class QQuickSwipePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickSwipe)

public:
    QQuickSwipePrivate(QQuickSwipeDelegate *control) :
        control(control),
        positionBeforePress(0),
        position(0),
        wasComplete(false),
        complete(false),
        left(nullptr),
        behind(nullptr),
        right(nullptr),
        leftItem(nullptr),
        behindItem(nullptr),
        rightItem(nullptr)
    {
    }

    static QQuickSwipePrivate *get(QQuickSwipe *swipe);

    QQuickItem *createDelegateItem(QQmlComponent *component);
    QQuickItem *showRelevantItemForPosition(qreal position);
    QQuickItem *createRelevantItemForDistance(qreal distance);
    void reposition(PositionAnimation animationPolicy);
    void createLeftItem();
    void createBehindItem();
    void createRightItem();
    void createAndShowLeftItem();
    void createAndShowBehindItem();
    void createAndShowRightItem();

    void warnAboutMixingDelegates();
    void warnAboutSettingDelegatesWhileVisible();

    bool hasDelegates() const;

    QQuickSwipeDelegate *control;
    // Same range as position, but is set before press events so that we can
    // keep track of which direction the user must swipe when using left and right delegates.
    qreal positionBeforePress;
    qreal position;
    // A "less strict" version of complete that is true if complete was true
    // before the last press event.
    bool wasComplete;
    bool complete;
    QQuickVelocityCalculator velocityCalculator;
    QQmlComponent *left;
    QQmlComponent *behind;
    QQmlComponent *right;
    QQuickItem *leftItem;
    QQuickItem *behindItem;
    QQuickItem *rightItem;
};

QQuickSwipePrivate *QQuickSwipePrivate::get(QQuickSwipe *swipe)
{
    return swipe->d_func();
}

QQuickItem *QQuickSwipePrivate::createDelegateItem(QQmlComponent *component)
{
    // If we don't use the correct context, it won't be possible to refer to
    // the control's id from within the delegates.
    QQmlContext *creationContext = component->creationContext();
    // The component might not have been created in QML, in which case
    // the creation context will be null and we have to create it ourselves.
    if (!creationContext)
        creationContext = qmlContext(control);
    QQmlContext *context = new QQmlContext(creationContext);
    context->setContextObject(control);
    QQuickItem *item = qobject_cast<QQuickItem*>(component->beginCreate(context));
    if (item) {
        item->setParentItem(control);
        component->completeCreate();
    }
    return item;
}

QQuickItem *QQuickSwipePrivate::showRelevantItemForPosition(qreal position)
{
    if (qFuzzyIsNull(position))
        return nullptr;

    if (behind) {
        createAndShowBehindItem();
        return behindItem;
    }

    if (right && position < 0.0) {
        createAndShowRightItem();
        return rightItem;
    }

    if (left && position > 0.0) {
        createAndShowLeftItem();
        return leftItem;
    }

    return nullptr;
}

QQuickItem *QQuickSwipePrivate::createRelevantItemForDistance(qreal distance)
{
    if (qFuzzyIsNull(distance))
        return nullptr;

    if (behind) {
        createBehindItem();
        return behindItem;
    }

    // a) If the position before the press was 0.0, we know that *any* movement
    // whose distance is negative will result in the right item being shown and
    // vice versa.
    // b) Once the control has been exposed (that is, swiped to the left or right,
    // and hence the position is either -1.0 or 1.0), we must use the width of the
    // relevant item to determine if the distance is larger than that item,
    // in order to know whether or not to display it.
    // c) If the control has been exposed, and the swipe is larger than the width
    // of the relevant item from which the swipe started from, we must show the
    // item on the other side (if any).

    if (right) {
        if ((distance < 0.0 && positionBeforePress == 0.0) /* a) */
            || (rightItem && positionBeforePress == -1.0 && distance < rightItem->width()) /* b) */
            || (leftItem && positionBeforePress == 1.0 && qAbs(distance) > leftItem->width())) /* c) */ {
            createRightItem();
            return rightItem;
        }
    }

    if (left) {
        if ((distance > 0.0 && positionBeforePress == 0.0) /* a) */
            || (leftItem && positionBeforePress == 1.0 && qAbs(distance) < leftItem->width()) /* b) */
            || (rightItem && positionBeforePress == -1.0 && qAbs(distance) > rightItem->width())) /* c) */ {
            createLeftItem();
            return leftItem;
        }
    }

    return nullptr;
}

void QQuickSwipePrivate::reposition(PositionAnimation animationPolicy)
{
    QQuickItem *relevantItem = showRelevantItemForPosition(position);
    const qreal relevantWidth = relevantItem ? relevantItem->width() : 0.0;
    const qreal contentItemX = position * relevantWidth + control->leftPadding();

    // "Behavior on x" relies on the property system to know when it should update,
    // so we can prevent it from animating by setting the x position directly.
    if (animationPolicy == AnimatePosition) {
        if (QQuickItem *contentItem = control->contentItem())
            contentItem->setProperty("x", contentItemX);
        if (QQuickItem *background = control->background())
            background->setProperty("x", position * relevantWidth);
    } else {
        if (QQuickItem *contentItem = control->contentItem())
            contentItem->setX(contentItemX);
        if (QQuickItem *background = control->background())
            background->setX(position * relevantWidth);
    }
}

void QQuickSwipePrivate::createLeftItem()
{
    if (!leftItem) {
        Q_Q(QQuickSwipe);
        q->setLeftItem(createDelegateItem(left));
        if (!leftItem)
            qmlInfo(control) << "Failed to create left item:" << left->errors();
    }
}

void QQuickSwipePrivate::createBehindItem()
{
    if (!behindItem) {
        Q_Q(QQuickSwipe);
        q->setBehindItem(createDelegateItem(behind));
        if (!behindItem)
            qmlInfo(control) << "Failed to create behind item:" << behind->errors();
    }
}

void QQuickSwipePrivate::createRightItem()
{
    if (!rightItem) {
        Q_Q(QQuickSwipe);
        q->setRightItem(createDelegateItem(right));
        if (!rightItem)
            qmlInfo(control) << "Failed to create right item:" << right->errors();
    }
}

void QQuickSwipePrivate::createAndShowLeftItem()
{
    createLeftItem();

    if (leftItem)
        leftItem->setVisible(true);

    if (rightItem)
        rightItem->setVisible(false);
}

void QQuickSwipePrivate::createAndShowBehindItem()
{
    createBehindItem();

    if (behindItem)
        behindItem->setVisible(true);
}

void QQuickSwipePrivate::createAndShowRightItem()
{
    createRightItem();

    // This item may have already existed but was hidden.
    if (rightItem)
        rightItem->setVisible(true);

    // The left item isn't visible when the right item is visible, so save rendering effort by hiding it.
    if (leftItem)
        leftItem->setVisible(false);
}

void QQuickSwipePrivate::warnAboutMixingDelegates()
{
    qmlInfo(control) << "cannot set both behind and left/right properties";
}

void QQuickSwipePrivate::warnAboutSettingDelegatesWhileVisible()
{
    qmlInfo(control) << "left/right/behind properties may only be set when swipe.position is 0";
}

bool QQuickSwipePrivate::hasDelegates() const
{
    return left || right || behind;
}

QQuickSwipe::QQuickSwipe(QQuickSwipeDelegate *control) :
    QObject(*(new QQuickSwipePrivate(control)))
{
}

QQmlComponent *QQuickSwipe::left() const
{
    Q_D(const QQuickSwipe);
    return d->left;
}

void QQuickSwipe::setLeft(QQmlComponent *left)
{
    Q_D(QQuickSwipe);
    if (left == d->left)
        return;

    if (d->behind) {
        d->warnAboutMixingDelegates();
        return;
    }

    if (!qFuzzyIsNull(d->position)) {
        d->warnAboutSettingDelegatesWhileVisible();
        return;
    }

    d->left = left;

    if (!d->left) {
        delete d->leftItem;
        d->leftItem = nullptr;
    }

    d->control->setFiltersChildMouseEvents(d->hasDelegates());

    emit leftChanged();
}

QQmlComponent *QQuickSwipe::behind() const
{
    Q_D(const QQuickSwipe);
    return d->behind;
}

void QQuickSwipe::setBehind(QQmlComponent *behind)
{
    Q_D(QQuickSwipe);
    if (behind == d->behind)
        return;

    if (d->left || d->right) {
        d->warnAboutMixingDelegates();
        return;
    }

    if (!qFuzzyIsNull(d->position)) {
        d->warnAboutSettingDelegatesWhileVisible();
        return;
    }

    d->behind = behind;

    if (!d->behind) {
        delete d->behindItem;
        d->behindItem = nullptr;
    }

    d->control->setFiltersChildMouseEvents(d->hasDelegates());

    emit behindChanged();
}

QQmlComponent *QQuickSwipe::right() const
{
    Q_D(const QQuickSwipe);
    return d->right;
}

void QQuickSwipe::setRight(QQmlComponent *right)
{
    Q_D(QQuickSwipe);
    if (right == d->right)
        return;

    if (d->behind) {
        d->warnAboutMixingDelegates();
        return;
    }

    if (!qFuzzyIsNull(d->position)) {
        d->warnAboutSettingDelegatesWhileVisible();
        return;
    }

    d->right = right;

    if (!d->right) {
        delete d->rightItem;
        d->rightItem = nullptr;
    }

    d->control->setFiltersChildMouseEvents(d->hasDelegates());

    emit rightChanged();
}

QQuickItem *QQuickSwipe::leftItem() const
{
    Q_D(const QQuickSwipe);
    return d->leftItem;
}

void QQuickSwipe::setLeftItem(QQuickItem *item)
{
    Q_D(QQuickSwipe);
    if (item == d->leftItem)
        return;

    delete d->leftItem;
    d->leftItem = item;

    if (d->leftItem) {
        d->leftItem->setParentItem(d->control);

        if (qFuzzyIsNull(d->leftItem->z()))
            d->leftItem->setZ(-2);
    }

    emit leftItemChanged();
}

QQuickItem *QQuickSwipe::behindItem() const
{
    Q_D(const QQuickSwipe);
    return d->behindItem;
}

void QQuickSwipe::setBehindItem(QQuickItem *item)
{
    Q_D(QQuickSwipe);
    if (item == d->behindItem)
        return;

    delete d->behindItem;
    d->behindItem = item;

    if (d->behindItem) {
        d->behindItem->setParentItem(d->control);

        if (qFuzzyIsNull(d->behindItem->z()))
            d->behindItem->setZ(-2);
    }

    emit behindItemChanged();
}

QQuickItem *QQuickSwipe::rightItem() const
{
    Q_D(const QQuickSwipe);
    return d->rightItem;
}

void QQuickSwipe::setRightItem(QQuickItem *item)
{
    Q_D(QQuickSwipe);
    if (item == d->rightItem)
        return;

    delete d->rightItem;
    d->rightItem = item;

    if (d->rightItem) {
        d->rightItem->setParentItem(d->control);

        if (qFuzzyIsNull(d->rightItem->z()))
            d->rightItem->setZ(-2);
    }

    emit rightItemChanged();
}

qreal QQuickSwipe::position() const
{
    Q_D(const QQuickSwipe);
    return d->position;
}

void QQuickSwipe::setPosition(qreal position)
{
    Q_D(QQuickSwipe);
    const qreal adjustedPosition = qBound<qreal>(-1.0, position, 1.0);
    if (adjustedPosition == d->position)
        return;

    d->position = adjustedPosition;
    d->reposition(AnimatePosition);
    emit positionChanged();
}

bool QQuickSwipe::isComplete() const
{
    Q_D(const QQuickSwipe);
    return d->complete;
}

void QQuickSwipe::setComplete(bool complete)
{
    Q_D(QQuickSwipe);
    if (complete == d->complete)
        return;

    d->complete = complete;
    emit completeChanged();
    if (d->complete)
        emit completed();
}

void QQuickSwipe::close()
{
    Q_D(QQuickSwipe);
    setPosition(0);
    setComplete(false);
    d->wasComplete = false;
    d->positionBeforePress = 0.0;
    d->velocityCalculator.reset();
}

QQuickSwipeDelegatePrivate::QQuickSwipeDelegatePrivate(QQuickSwipeDelegate *control) :
    swipe(control)
{
}

bool QQuickSwipeDelegatePrivate::handleMousePressEvent(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickSwipeDelegate);
    QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&swipe);
    // If the position is 0, we want to handle events ourselves - we don't want child items to steal them.
    // This code will only get called when a child item has been created;
    // events will go through the regular channels (mousePressEvent()) until then.
    if (qFuzzyIsNull(swipePrivate->position)) {
        q->mousePressEvent(event);
        // The press point could be incorrect if the press happened over a child item,
        // so we correct it after calling the base class' mousePressEvent(), rather
        // than having to duplicate its code just so we can set the pressPoint.
        pressPoint = item->mapToItem(q, event->pos());
        return true;
    }

    // The position is non-zero, this press could be either for a delegate or the control itself
    // (the control can be clicked to e.g. close the swipe). Either way, we must begin measuring
    // mouse movement in case it turns into a swipe, in which case we grab the mouse.
    swipePrivate->positionBeforePress = swipePrivate->position;
    swipePrivate->velocityCalculator.startMeasuring(event->pos(), event->timestamp());
    pressPoint = item->mapToItem(q, event->pos());

    // When a delegate uses the attached properties and signals, it declares that it wants mouse events.
    Attached *attached = attachedObject(item);
    if (attached) {
        attached->setPressed(true);
        // Stop the event from propagating, as QQuickItem explicitly ignores events.
        event->accept();
        return true;
    }

    return false;
}

bool QQuickSwipeDelegatePrivate::handleMouseMoveEvent(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickSwipeDelegate);

    if (holdTimer > 0) {
        if (QLineF(pressPoint, event->localPos()).length() > QGuiApplication::styleHints()->startDragDistance())
            stopPressAndHold();
    }

    // Protect against division by zero.
    if (width == 0)
        return false;

    // Don't bother reacting to events if we don't have any delegates.
    QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&swipe);
    if (!swipePrivate->left && !swipePrivate->right && !swipePrivate->behind)
        return false;

    // Don't handle move events for the control if it wasn't pressed.
    if (item == q && !pressed)
        return false;

    const QPointF mappedEventPos = item->mapToItem(q, event->pos());
    const qreal distance = (mappedEventPos - pressPoint).x();
    if (!q->keepMouseGrab()) {
        // Taken from QQuickDrawerPrivate::grabMouse; see comments there.
        int threshold = qMax(20, QGuiApplication::styleHints()->startDragDistance() + 5);
        const bool overThreshold = QQuickWindowPrivate::dragOverThreshold(distance, Qt::XAxis, event, threshold);
        if (window && overThreshold) {
            QQuickItem *grabber = q->window()->mouseGrabberItem();
            if (!grabber || !grabber->keepMouseGrab()) {
                q->grabMouse();
                q->setKeepMouseGrab(true);
                q->setPressed(true);
                swipe.setComplete(false);

                if (Attached *attached = attachedObject(item))
                    attached->setPressed(false);
            }
        }
    }

    if (q->keepMouseGrab()) {
        // Ensure we don't try to calculate a position when the user tried to drag
        // to the left when the left item is already exposed, and vice versa.
        // The code below assumes that the drag is valid, so if we don't have this check,
        // the wrong items are visible and the swiping wraps.
        if (swipePrivate->behind
            || ((swipePrivate->left || swipePrivate->right)
                && (qFuzzyIsNull(swipePrivate->positionBeforePress)
                    || (swipePrivate->positionBeforePress == -1.0 && distance >= 0.0)
                    || (swipePrivate->positionBeforePress == 1.0 && distance <= 0.0)))) {

            // We must instantiate the items here so that we can calculate the
            // position against the width of the relevant item.
            QQuickItem *relevantItem = swipePrivate->createRelevantItemForDistance(distance);
            // If there isn't any relevant item, the user may have swiped back to the 0 position,
            // or they swiped back to a position that is equal to positionBeforePress.
            const qreal normalizedDistance = relevantItem ? distance / relevantItem->width() : 0.0;
            qreal position = 0;

            // If the control was exposed before the drag begun, the distance should be inverted.
            // For example, if the control had been swiped to the right, the position would be 1.0.
            // If the control was then swiped to the left by a distance of -20 pixels, the normalized
            // distance might be -0.2, for example, which cannot be used as the position; the swipe
            // started from the right, so we account for that by adding the position.
            if (qFuzzyIsNull(normalizedDistance)) {
                // There are two cases when the normalizedDistance can be 0,
                // and we must distinguish between them:
                //
                // a) The swipe returns to the position that it was at before the press event.
                // In this case, the distance will be 0.
                // There would have been many position changes in the meantime, so we can't just
                // ignore the move event; we have to set position to what it was before the press.
                //
                // b) If the position was at, 1.0, for example, and the control was then swiped
                // to the left by the exact width of the left item, there won't be any relevant item
                // (because the swipe's position would be at 0.0). In turn, the normalizedDistance
                // would be 0 (because of the lack of a relevant item), but the distance will be non-zero.
                position = qFuzzyIsNull(distance) ? swipePrivate->positionBeforePress : 0;
            } else if (!swipePrivate->wasComplete) {
                position = normalizedDistance;
            } else {
                position = distance > 0 ? normalizedDistance - 1.0 : normalizedDistance + 1.0;
            }

            swipe.setPosition(position);
        }
    } else {
        // The swipe wasn't initiated.
        if (event->pos().y() < 0 || event->pos().y() > height) {
            // The mouse went outside the vertical bounds of the control, so
            // we should no longer consider it pressed.
            q->setPressed(false);
        }
    }

    event->accept();

    return q->keepMouseGrab();
}

static const qreal exposeVelocityThreshold = 300.0;

bool QQuickSwipeDelegatePrivate::handleMouseReleaseEvent(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickSwipeDelegate);
    QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&swipe);
    swipePrivate->velocityCalculator.stopMeasuring(event->pos(), event->timestamp());

    const bool hadGrabbedMouse = q->keepMouseGrab();
    q->setKeepMouseGrab(false);

    // The control can be exposed by either swiping past the halfway mark, or swiping fast enough.
    const qreal swipeVelocity = swipePrivate->velocityCalculator.velocity().x();
    if (swipePrivate->position > 0.5 ||
        (swipePrivate->position > 0.0 && swipeVelocity > exposeVelocityThreshold)) {
        swipe.setPosition(1.0);
        swipe.setComplete(true);
        swipePrivate->wasComplete = true;
    } else if (swipePrivate->position < -0.5 ||
        (swipePrivate->position < 0.0 && swipeVelocity < -exposeVelocityThreshold)) {
        swipe.setPosition(-1.0);
        swipe.setComplete(true);
        swipePrivate->wasComplete = true;
    } else {
        swipe.setPosition(0.0);
        swipe.setComplete(false);
        swipePrivate->wasComplete = false;
    }

    if (Attached *attached = attachedObject(item)) {
        const bool wasPressed = attached->isPressed();
        if (wasPressed) {
            attached->setPressed(false);
            emit attached->clicked();
        }
    }

    // Only consume child events if we had grabbed the mouse.
    return hadGrabbedMouse;
}

static void warnIfHorizontallyAnchored(QQuickItem *item, const QString &itemName)
{
    if (!item)
        return;

    QQuickAnchors *anchors = QQuickItemPrivate::get(item)->_anchors;
    if (anchors && (anchors->fill() || anchors->centerIn() || anchors->left().item || anchors->right().item)
            && !item->property("_q_QQuickSwipeDelegate_warned").toBool()) {
        qmlInfo(item) << QString::fromLatin1("SwipeDelegate: cannot use horizontal anchors with %1; unable to layout the item.").arg(itemName);
        item->setProperty("_q_QQuickSwipeDelegate_warned", true);
    }
}

void QQuickSwipeDelegatePrivate::resizeContent()
{
    warnIfHorizontallyAnchored(background, QStringLiteral("background"));
    warnIfHorizontallyAnchored(contentItem, QStringLiteral("contentItem"));

    // If the background and contentItem are repositioned due to a swipe,
    // we don't want to call QQuickControlPrivate's implementation of this function,
    // as it repositions the contentItem to be visible.
    // However, we still want to resize the control vertically.
    QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&swipe);
    if (!swipePrivate->complete) {
        QQuickItemDelegatePrivate::resizeContent();
    } else if (contentItem) {
        Q_Q(QQuickSwipeDelegate);
        contentItem->setY(q->topPadding());
        contentItem->setHeight(q->availableHeight());
    }
}

QQuickSwipeDelegate::QQuickSwipeDelegate(QQuickItem *parent) :
    QQuickItemDelegate(*(new QQuickSwipeDelegatePrivate(this)), parent)
{
}

/*!
    \since QtQuick.Controls 2.1
    \qmlmethod void QtQuick.Controls::SwipeDelegate::swipe.close()

    This method sets the \c position of the swipe to \c 0. Any animations
    defined for the \l {Item::}{x} position of \l {Control::}{contentItem}
    and \l {Control::}{background} will be triggered.

    \sa swipe
*/

/*!
    \qmlpropertygroup QtQuick.Controls::SwipeDelegate::swipe
    \qmlproperty real QtQuick.Controls::SwipeDelegate::swipe.position
    \qmlproperty bool QtQuick.Controls::SwipeDelegate::swipe.complete
    \qmlproperty Component QtQuick.Controls::SwipeDelegate::swipe.left
    \qmlproperty Component QtQuick.Controls::SwipeDelegate::swipe.behind
    \qmlproperty Component QtQuick.Controls::SwipeDelegate::swipe.right
    \qmlproperty Item QtQuick.Controls::SwipeDelegate::swipe.leftItem
    \qmlproperty Item QtQuick.Controls::SwipeDelegate::swipe.behindItem
    \qmlproperty Item QtQuick.Controls::SwipeDelegate::swipe.rightItem

    \table
    \header
        \li Name
        \li Description
    \row
        \li position
        \li This read-only property holds the position of the swipe relative to either
            side of the control. When this value reaches either
            \c -1.0 (left side) or \c 1.0 (right side) and the mouse button is
            released, \c complete will be \c true.
    \row
        \li complete
        \li This read-only property holds whether the control is fully exposed after
            having been swiped to the left or right.

            When complete is \c true, any interactive items declared in \c left,
            \c right, or \c behind will receive mouse events.
    \row
        \li left
        \li This property holds the left delegate.

            The left delegate sits behind both \l {Control::}{contentItem} and
            \l {Control::}{background}. When the SwipeDelegate is swiped to the right,
            this item will be gradually revealed.

            \include qquickswipedelegate-interaction.qdocinc
    \row
        \li behind
        \li This property holds the delegate that is shown when the
            SwipeDelegate is swiped to both the left and right.

            As with the \c left and \c right delegates, it sits behind both
            \l {Control::}{contentItem} and \l {Control::}{background}. However, a
            SwipeDelegate whose \c behind has been set can be continuously swiped
            from either side, and will always show the same item.

            \include qquickswipedelegate-interaction.qdocinc
    \row
        \li right
        \li This property holds the right delegate.

            The right delegate sits behind both \l {Control::}{contentItem} and
            \l {Control::}{background}. When the SwipeDelegate is swiped to the left,
            this item will be gradually revealed.

            \include qquickswipedelegate-interaction.qdocinc
    \row
        \li leftItem
        \li This read-only property holds the item instantiated from the \c left component.

            If \c left has not been set, or the position hasn't changed since
            creation of the SwipeDelegate, this property will be \c null.
    \row
        \li behindItem
        \li This read-only property holds the item instantiated from the \c behind component.

            If \c behind has not been set, or the position hasn't changed since
            creation of the SwipeDelegate, this property will be \c null.
    \row
        \li rightItem
        \li This read-only property holds the item instantiated from the \c right component.

            If \c right has not been set, or the position hasn't changed since
            creation of the SwipeDelegate, this property will be \c null.
    \row
        \li completed()
        \li This signal is emitted when \c complete becomes \c true.

            It is useful for performing some action upon completion of a swipe.
            For example, it can be used to remove the delegate from the list
            that it is in.

            This signal was added in QtQuick.Controls 2.1.
    \endtable

    \sa {Control::}{contentItem}, {Control::}{background}, swipe.close()
*/
QQuickSwipe *QQuickSwipeDelegate::swipe() const
{
    Q_D(const QQuickSwipeDelegate);
    return const_cast<QQuickSwipe*>(&d->swipe);
}

QQuickSwipeDelegateAttached *QQuickSwipeDelegate::qmlAttachedProperties(QObject *object)
{
    return new QQuickSwipeDelegateAttached(object);
}

static bool isChildOrGrandchildOf(QQuickItem *child, QQuickItem *item)
{
    return item && (child == item || item->isAncestorOf(child));
}

bool QQuickSwipeDelegate::childMouseEventFilter(QQuickItem *child, QEvent *event)
{
    Q_D(QQuickSwipeDelegate);
    // The contentItem is, by default, usually a non-interactive item like Text, and
    // the same applies to the background. This means that simply stacking the left/right/behind
    // items before these items won't allow us to get mouse events when the control is not currently exposed
    // but has been previously. Therefore, we instead call setFiltersChildMouseEvents(true) in the constructor
    // and filter out child events only when the child is the left/right/behind item.
    const QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&d->swipe);
    if (!isChildOrGrandchildOf(child, swipePrivate->leftItem) && !isChildOrGrandchildOf(child, swipePrivate->behindItem)
        && !isChildOrGrandchildOf(child, swipePrivate->rightItem)) {
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        return d->handleMousePressEvent(child, static_cast<QMouseEvent *>(event));
    } case QEvent::MouseMove: {
        return d->handleMouseMoveEvent(child, static_cast<QMouseEvent *>(event));
    } case QEvent::MouseButtonRelease: {
        // Make sure that the control gets release events if it has created child
        // items that are stealing events from it.
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        QQuickItemDelegate::mouseReleaseEvent(mouseEvent);
        return d->handleMouseReleaseEvent(child, mouseEvent);
    } case QEvent::UngrabMouse: {
        // If the mouse was pressed over e.g. rightItem and then dragged down,
        // the ListView would eventually grab the mouse, at which point we must
        // clear the pressed flag so that it doesn't stay pressed after the release.
        Attached *attached = attachedObject(child);
        if (attached)
            attached->setPressed(false);
        return false;
    } default:
        return false;
    }
}

// We only override this to set positionBeforePress;
// otherwise, it's the same as the base class implementation.
void QQuickSwipeDelegate::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickSwipeDelegate);
    QQuickItemDelegate::mousePressEvent(event);
    QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&d->swipe);
    swipePrivate->positionBeforePress = swipePrivate->position;
    swipePrivate->velocityCalculator.startMeasuring(event->pos(), event->timestamp());
}

void QQuickSwipeDelegate::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickSwipeDelegate);
    if (filtersChildMouseEvents())
        d->handleMouseMoveEvent(this, event);
    else
        QQuickItemDelegate::mouseMoveEvent(event);
}

void QQuickSwipeDelegate::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickSwipeDelegate);
    if (!filtersChildMouseEvents() || !d->handleMouseReleaseEvent(this, event))
        QQuickItemDelegate::mouseReleaseEvent(event);
}

void QQuickSwipeDelegate::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickSwipeDelegate);
    QQuickControl::geometryChanged(newGeometry, oldGeometry);

    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width())) {
        QQuickSwipePrivate *swipePrivate = QQuickSwipePrivate::get(&d->swipe);
        swipePrivate->reposition(DontAnimatePosition);
    }
}

QFont QQuickSwipeDelegate::defaultFont() const
{
    return QQuickControlPrivate::themeFont(QPlatformTheme::ListViewFont);
}

#ifndef QT_NO_ACCESSIBILITY
QAccessible::Role QQuickSwipeDelegate::accessibleRole() const
{
    return QAccessible::ListItem;
}
#endif

class QQuickSwipeDelegateAttachedPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickSwipeDelegateAttached)

public:
    QQuickSwipeDelegateAttachedPrivate() :
        pressed(false)
    {
    }

    // True when left/right/behind is non-interactive and is pressed.
    bool pressed;
};

/*!
    \since QtQuick.Controls 2.1
    \qmlattachedsignal QtQuick.Controls::SwipeDelegate::clicked()

    This signal can be attached to a non-interactive item declared in
    \c swipe.left, \c swipe.right, or \c swipe.behind, in order to react to
    clicks. Items can only be clicked when \c swipe.complete is \c true.

    For interactive controls (such as \l Button) declared in these
    items, use their respective \c clicked() signal instead.

    To respond to clicks on the SwipeDelegate itself, use its
    \l {AbstractButton::}{clicked()} signal.

    \note See the documentation for \l pressed for information on
    how to use the event-related properties correctly.

    \sa pressed
*/

QQuickSwipeDelegateAttached::QQuickSwipeDelegateAttached(QObject *object) :
    QObject(*(new QQuickSwipeDelegateAttachedPrivate), object)
{
    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (item) {
        // This allows us to be notified when an otherwise non-interactive item
        // is pressed and clicked. The alternative is much more more complex:
        // iterating through children that contain the event pos and finding
        // the first one with an attached object.
        item->setAcceptedMouseButtons(Qt::AllButtons);
    } else {
        qWarning() << "Attached properties of SwipeDelegate must be accessed through an Item";
    }
}

/*!
    \since QtQuick.Controls 2.1
    \qmlattachedproperty bool QtQuick.Controls::SwipeDelegate::pressed
    \readonly

    This property can be attached to a non-interactive item declared in
    \c swipe.left, \c swipe.right, or \c swipe.behind, in order to detect if it
    is pressed. Items can only be pressed when \c swipe.complete is \c true.

    For example:

    \code
    swipe.right: Label {
        anchors.right: parent.right
        height: parent.height
        text: "Action"
        color: "white"
        padding: 12
        background: Rectangle {
            color: SwipeDelegate.pressed ? Qt.darker("tomato", 1.1) : "tomato"
        }
    }
    \endcode

    It is possible to have multiple items which individually receive mouse and
    touch events. For example, to have two actions in the \c swipe.right item,
    use the following code:

    \code
    swipe.right: Row {
        anchors.right: parent.right
        height: parent.height

        Label {
            id: moveLabel
            text: qsTr("Move")
            color: "white"
            verticalAlignment: Label.AlignVCenter
            padding: 12
            height: parent.height

            SwipeDelegate.onClicked: console.log("Moving...")

            background: Rectangle {
                color: moveLabel.SwipeDelegate.pressed ? Qt.darker("#ffbf47", 1.1) : "#ffbf47"
            }
        }
        Label {
            id: deleteLabel
            text: qsTr("Delete")
            color: "white"
            verticalAlignment: Label.AlignVCenter
            padding: 12
            height: parent.height

            SwipeDelegate.onClicked: console.log("Deleting...")

            background: Rectangle {
                color: deleteLabel.SwipeDelegate.pressed ? Qt.darker("tomato", 1.1) : "tomato"
            }
        }
    }
    \endcode

    Note how the \c color assignment in each \l {Control::}{background} item
    qualifies the attached property with the \c id of the label. This
    is important; using the attached properties on an item causes that item
    to accept events. Suppose we had left out the \c id in the previous example:

    \code
    color: SwipeDelegate.pressed ? Qt.darker("tomato", 1.1) : "tomato"
    \endcode

    The \l Rectangle background item is a child of the label, so it naturally
    receives events before it. In practice, this means that the background
    color will change, but the \c onClicked handler in the label will never
    get called.

    For interactive controls (such as \l Button) declared in these
    items, use their respective \c pressed property instead.

    For presses on the SwipeDelegate itself, use its
    \l {AbstractButton::}{pressed} property.

    \sa clicked()
*/
bool QQuickSwipeDelegateAttached::isPressed() const
{
    Q_D(const QQuickSwipeDelegateAttached);
    return d->pressed;
}

void QQuickSwipeDelegateAttached::setPressed(bool pressed)
{
    Q_D(QQuickSwipeDelegateAttached);
    if (pressed == d->pressed)
        return;

    d->pressed = pressed;
    emit pressedChanged();
}

QT_END_NAMESPACE
