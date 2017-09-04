/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qquickscrollbar_p.h"
#include "qquickscrollbar_p_p.h"
#include "qquickscrollview_p.h"

#include <QtQml/qqmlinfo.h>
#include <QtQuick/private/qquickflickable_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype ScrollBar
    \inherits Control
    \instantiates QQuickScrollBar
    \inqmlmodule QtQuick.Controls
    \since 5.7
    \ingroup qtquickcontrols2-indicators
    \brief Vertical or horizontal interactive scroll bar.

    \image qtquickcontrols2-scrollbar.gif

    ScrollBar is an interactive bar that can be used to scroll to a specific
    position. A scroll bar can be either \l vertical or \l horizontal, and can
    be attached to any \l Flickable, such as \l ListView and \l GridView.

    \code
    Flickable {
        // ...
        ScrollBar.vertical: ScrollBar { }
    }
    \endcode

    \section1 Attaching ScrollBar to a Flickable

    When ScrollBar is attached \l {ScrollBar::vertical}{vertically} or
    \l {ScrollBar::horizontal}{horizontally} to a Flickable, its geometry and
    the following properties are automatically set and updated as appropriate:

    \list
    \li \l orientation
    \li \l position
    \li \l size
    \li \l active
    \endlist

    An attached ScrollBar re-parents itself to the target Flickable. A vertically
    attached ScrollBar resizes itself to the height of the Flickable, and positions
    itself to either side of it based on the \l {Control::mirrored}{layout direction}.
    A horizontally attached ScrollBar resizes itself to the width of the Flickable,
    and positions itself to the bottom. The automatic geometry management can be disabled
    by specifying another parent for the attached ScrollBar. This can be useful, for
    example, if the ScrollBar should be placed outside a clipping Flickable. This is
    demonstrated by the following example:

    \code
    Flickable {
        id: flickable
        clip: true
        // ...
        ScrollBar.vertical: ScrollBar {
            parent: flickable.parent
            anchors.top: flickable.top
            anchors.left: flickable.right
            anchors.bottom: flickable.bottom
        }
    }
    \endcode

    Notice that ScrollBar does not filter key events of the Flickable it is
    attached to. The following example illustrates how to implement scrolling
    with up and down keys:

    \code
    Flickable {
        focus: true

        Keys.onUpPressed: scrollBar.decrease()
        Keys.onDownPressed: scrollBar.increase()

        ScrollBar.vertical: ScrollBar { id: scrollBar }
    }
    \endcode

    \section1 Binding the Active State of Horizontal and Vertical Scroll Bars

    Horizontal and vertical scroll bars do not share the \l active state with
    each other by default. In order to keep both bars visible whilst scrolling
    to either direction, establish a two-way binding between the active states
    as presented by the following example:

    \snippet qtquickcontrols2-scrollbar-active.qml 1

    \section1 Non-attached Scroll Bars

    It is possible to create an instance of ScrollBar without using the
    attached property API. This is useful when the behavior of the attached
    scoll bar is not sufficient or a \l Flickable is not in use. In the
    following example, horizontal and vertical scroll bars are used to
    scroll over the text without using \l Flickable:

    \snippet qtquickcontrols2-scrollbar-non-attached.qml 1

    \image qtquickcontrols2-scrollbar-non-attached.png

    When using a non-attached ScrollBar, the following must be done manually:

    \list
    \li Layout the scroll bar (with the \l {Item::}{x} and \l {Item::}{y} or
        \l {Item::anchors}{anchor} properties, for example).
    \li Set the \l size and \l position properties to determine the size and position
        of the scroll bar in relation to the scrolled item.
    \li Set the \l active property to determine when the scroll bar will be
        visible.
    \endlist

    \sa ScrollIndicator, {Customizing ScrollBar}, {Indicator Controls}
*/

static const QQuickItemPrivate::ChangeTypes changeTypes = QQuickItemPrivate::Geometry | QQuickItemPrivate::Destroyed;
static const QQuickItemPrivate::ChangeTypes horizontalChangeTypes = changeTypes | QQuickItemPrivate::ImplicitHeight;
static const QQuickItemPrivate::ChangeTypes verticalChangeTypes = changeTypes | QQuickItemPrivate::ImplicitWidth;

QQuickScrollBarPrivate::QQuickScrollBarPrivate()
    : size(0),
      position(0),
      stepSize(0),
      offset(0),
      active(false),
      pressed(false),
      moving(false),
      interactive(true),
      explicitInteractive(false),
      orientation(Qt::Vertical),
      snapMode(QQuickScrollBar::NoSnap),
      policy(QQuickScrollBar::AsNeeded)
{
}

qreal QQuickScrollBarPrivate::snapPosition(qreal position) const
{
    const qreal effectiveStep = stepSize * (1.0 - size);
    if (qFuzzyIsNull(effectiveStep))
        return position;

    return qRound(position / effectiveStep) * effectiveStep;
}

qreal QQuickScrollBarPrivate::positionAt(const QPointF &point) const
{
    Q_Q(const QQuickScrollBar);
    if (orientation == Qt::Horizontal)
        return (point.x() - q->leftPadding()) / q->availableWidth();
    else
        return (point.y() - q->topPadding()) / q->availableHeight();
}

void QQuickScrollBarPrivate::setInteractive(bool enabled)
{
    Q_Q(QQuickScrollBar);
    if (interactive == enabled)
        return;

    interactive = enabled;
    if (interactive) {
        q->setAcceptedMouseButtons(Qt::LeftButton);
#if QT_CONFIG(cursor)
        q->setCursor(Qt::ArrowCursor);
#endif
    } else {
        q->setAcceptedMouseButtons(Qt::NoButton);
#if QT_CONFIG(cursor)
        q->unsetCursor();
#endif
        q->ungrabMouse();
    }
    emit q->interactiveChanged();
}

void QQuickScrollBarPrivate::updateActive()
{
    Q_Q(QQuickScrollBar);
#if QT_CONFIG(quicktemplates2_hover)
    bool hover = hovered;
#else
    bool hover = false;
#endif
    q->setActive(moving || (interactive && (pressed || hover)));
}

void QQuickScrollBarPrivate::resizeContent()
{
    Q_Q(QQuickScrollBar);
    if (!contentItem)
        return;

    // - negative overshoot (pos < 0): clamp the pos to 0, and deduct the overshoot from the size
    // - positive overshoot (pos + size > 1): clamp the size to 1-pos
    const qreal clampedSize = qBound<qreal>(0, size + qMin<qreal>(0, position), 1.0 - position);
    const qreal clampedPos = qBound<qreal>(0, position, 1.0 - clampedSize);

    if (orientation == Qt::Horizontal) {
        contentItem->setPosition(QPointF(q->leftPadding() + clampedPos * q->availableWidth(), q->topPadding()));
        contentItem->setSize(QSizeF(q->availableWidth() * clampedSize, q->availableHeight()));
    } else {
        contentItem->setPosition(QPointF(q->leftPadding(), q->topPadding() + clampedPos * q->availableHeight()));
        contentItem->setSize(QSizeF(q->availableWidth(), q->availableHeight() * clampedSize));
    }
}

void QQuickScrollBarPrivate::handlePress(const QPointF &point)
{
    Q_Q(QQuickScrollBar);
    QQuickControlPrivate::handlePress(point);
    offset = positionAt(point) - position;
    if (offset < 0 || offset > size)
        offset = size / 2;
    q->setPressed(true);
}

void QQuickScrollBarPrivate::handleMove(const QPointF &point)
{
    Q_Q(QQuickScrollBar);
    QQuickControlPrivate::handleMove(point);
    qreal pos = qBound<qreal>(0.0, positionAt(point) - offset, 1.0 - size);
    if (snapMode == QQuickScrollBar::SnapAlways)
        pos = snapPosition(pos);
    q->setPosition(pos);
}

void QQuickScrollBarPrivate::handleRelease(const QPointF &point)
{
    Q_Q(QQuickScrollBar);
    QQuickControlPrivate::handleRelease(point);
    qreal pos = qBound<qreal>(0.0, positionAt(point) - offset, 1.0 - size);
    if (snapMode != QQuickScrollBar::NoSnap)
        pos = snapPosition(pos);
    q->setPosition(pos);
    offset = 0.0;
    q->setPressed(false);
}

void QQuickScrollBarPrivate::handleUngrab()
{
    Q_Q(QQuickScrollBar);
    QQuickControlPrivate::handleUngrab();
    offset = 0.0;
    q->setPressed(false);
}

QQuickScrollBar::QQuickScrollBar(QQuickItem *parent)
    : QQuickControl(*(new QQuickScrollBarPrivate), parent)
{
    setKeepMouseGrab(true);
    setAcceptedMouseButtons(Qt::LeftButton);
#if QT_CONFIG(cursor)
    setCursor(Qt::ArrowCursor);
#endif
}

QQuickScrollBarAttached *QQuickScrollBar::qmlAttachedProperties(QObject *object)
{
    return new QQuickScrollBarAttached(object);
}

/*!
    \qmlproperty real QtQuick.Controls::ScrollBar::size

    This property holds the size of the scroll bar, scaled to \c {0.0 - 1.0}.

    \sa {Flickable::visibleArea.heightRatio}{Flickable::visibleArea}

    This property is automatically set when the scroll bar is
    \l {Attaching ScrollBar to a Flickable}{attached to a flickable}.
*/
qreal QQuickScrollBar::size() const
{
    Q_D(const QQuickScrollBar);
    return d->size;
}

void QQuickScrollBar::setSize(qreal size)
{
    Q_D(QQuickScrollBar);
    if (qFuzzyCompare(d->size, size))
        return;

    d->size = size;
    if (isComponentComplete())
        d->resizeContent();
    emit sizeChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::ScrollBar::position

    This property holds the position of the scroll bar, scaled to \c {0.0 - 1.0}.

    \sa {Flickable::visibleArea.yPosition}{Flickable::visibleArea}

    This property is automatically set when the scroll bar is
    \l {Attaching ScrollBar to a Flickable}{attached to a flickable}.
*/
qreal QQuickScrollBar::position() const
{
    Q_D(const QQuickScrollBar);
    return d->position;
}

void QQuickScrollBar::setPosition(qreal position)
{
    Q_D(QQuickScrollBar);
    if (qFuzzyCompare(d->position, position))
        return;

    d->position = position;
    if (isComponentComplete())
        d->resizeContent();
    emit positionChanged();
}

/*!
    \qmlproperty real QtQuick.Controls::ScrollBar::stepSize

    This property holds the step size. The default value is \c 0.0.

    \sa snapMode, increase(), decrease()
*/
qreal QQuickScrollBar::stepSize() const
{
    Q_D(const QQuickScrollBar);
    return d->stepSize;
}

void QQuickScrollBar::setStepSize(qreal step)
{
    Q_D(QQuickScrollBar);
    if (qFuzzyCompare(d->stepSize, step))
        return;

    d->stepSize = step;
    emit stepSizeChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::ScrollBar::active

    This property holds whether the scroll bar is active, i.e. when it's \l pressed
    or the attached Flickable is \l {Flickable::moving}{moving}.

    It is possible to keep \l {Binding the Active State of Horizontal and Vertical Scroll Bars}
    {both horizontal and vertical bars visible} while scrolling in either direction.

    This property is automatically set when the scroll bar is
    \l {Attaching ScrollBar to a Flickable}{attached to a flickable}.
*/
bool QQuickScrollBar::isActive() const
{
    Q_D(const QQuickScrollBar);
    return d->active;
}

void QQuickScrollBar::setActive(bool active)
{
    Q_D(QQuickScrollBar);
    if (d->active == active)
        return;

    d->active = active;
    emit activeChanged();
}

/*!
    \qmlproperty bool QtQuick.Controls::ScrollBar::pressed

    This property holds whether the scroll bar is pressed.
*/
bool QQuickScrollBar::isPressed() const
{
    Q_D(const QQuickScrollBar);
    return d->pressed;
}

void QQuickScrollBar::setPressed(bool pressed)
{
    Q_D(QQuickScrollBar);
    if (d->pressed == pressed)
        return;

    d->pressed = pressed;
    setAccessibleProperty("pressed", pressed);
    d->updateActive();
    emit pressedChanged();
}

/*!
    \qmlproperty enumeration QtQuick.Controls::ScrollBar::orientation

    This property holds the orientation of the scroll bar.

    Possible values:
    \value Qt.Horizontal Horizontal
    \value Qt.Vertical Vertical (default)

    This property is automatically set when the scroll bar is
    \l {Attaching ScrollBar to a Flickable}{attached to a flickable}.
*/
Qt::Orientation QQuickScrollBar::orientation() const
{
    Q_D(const QQuickScrollBar);
    return d->orientation;
}

void QQuickScrollBar::setOrientation(Qt::Orientation orientation)
{
    Q_D(QQuickScrollBar);
    if (d->orientation == orientation)
        return;

    d->orientation = orientation;
    if (isComponentComplete())
        d->resizeContent();
    emit orientationChanged();
}

/*!
    \since QtQuick.Controls 2.2 (Qt 5.9)
    \qmlproperty enumeration QtQuick.Controls::ScrollBar::snapMode

    This property holds the snap mode.

    Possible values:
    \value ScrollBar.NoSnap The scrollbar does not snap (default).
    \value ScrollBar.SnapAlways The scrollbar snaps while dragged.
    \value ScrollBar.SnapOnRelease The scrollbar does not snap while being dragged, but only after released.

    In the following table, the various modes are illustrated with animations.
    The movement and the \l stepSize (\c 0.25) are identical in each animation.

    \table
    \header
        \row \li \b Value \li \b Example
        \row \li \c ScrollBar.NoSnap \li \image qtquickcontrols2-scrollbar-nosnap.gif
        \row \li \c ScrollBar.SnapAlways \li \image qtquickcontrols2-scrollbar-snapalways.gif
        \row \li \c ScrollBar.SnapOnRelease \li \image qtquickcontrols2-scrollbar-snaponrelease.gif
    \endtable

    \sa stepSize
*/
QQuickScrollBar::SnapMode QQuickScrollBar::snapMode() const
{
    Q_D(const QQuickScrollBar);
    return d->snapMode;
}

void QQuickScrollBar::setSnapMode(SnapMode mode)
{
    Q_D(QQuickScrollBar);
    if (d->snapMode == mode)
        return;

    d->snapMode = mode;
    emit snapModeChanged();
}

/*!
    \since QtQuick.Controls 2.2 (Qt 5.9)
    \qmlproperty bool QtQuick.Controls::ScrollBar::interactive

    This property holds whether the scroll bar is interactive. The default value is \c true.

    A non-interactive scroll bar is visually and behaviorally similar to \l ScrollIndicator.
    This property is useful for switching between typical mouse- and touch-orientated UIs
    with interactive and non-interactive scroll bars, respectively.
*/
bool QQuickScrollBar::isInteractive() const
{
    Q_D(const QQuickScrollBar);
    return d->interactive;
}

void QQuickScrollBar::setInteractive(bool interactive)
{
    Q_D(QQuickScrollBar);
    d->explicitInteractive = true;
    d->setInteractive(interactive);
}

void QQuickScrollBar::resetInteractive()
{
    Q_D(QQuickScrollBar);
    d->explicitInteractive = false;
    d->setInteractive(true);
}

/*!
    \since QtQuick.Controls 2.2 (Qt 5.9)
    \qmlproperty enumeration QtQuick.Controls::ScrollBar::policy

    This property holds the policy of the scroll bar. The default policy is \c ScrollBar.AsNeeded.

    Possible values:
    \value ScrollBar.AsNeeded The scroll bar is only shown when the content is too large to fit.
    \value ScrollBar.AlwaysOff The scroll bar is never shown.
    \value ScrollBar.AlwaysOn The scroll bar is always shown.

    The following example keeps the vertical scroll bar always visible:

    \snippet qtquickcontrols2-scrollbar-policy.qml 1
*/
QQuickScrollBar::Policy QQuickScrollBar::policy() const
{
    Q_D(const QQuickScrollBar);
    return d->policy;
}

void QQuickScrollBar::setPolicy(Policy policy)
{
    Q_D(QQuickScrollBar);
    if (d->policy == policy)
        return;

    d->policy = policy;
    emit policyChanged();
}

/*!
    \qmlmethod void QtQuick.Controls::ScrollBar::increase()

    Increases the position by \l stepSize or \c 0.1 if stepSize is \c 0.0.

    \sa stepSize
*/
void QQuickScrollBar::increase()
{
    Q_D(QQuickScrollBar);
    qreal step = qFuzzyIsNull(d->stepSize) ? 0.1 : d->stepSize;
    bool wasActive = d->active;
    setActive(true);
    setPosition(qMin<qreal>(1.0 - d->size, d->position + step));
    setActive(wasActive);
}

/*!
    \qmlmethod void QtQuick.Controls::ScrollBar::decrease()

    Decreases the position by \l stepSize or \c 0.1 if stepSize is \c 0.0.

    \sa stepSize
*/
void QQuickScrollBar::decrease()
{
    Q_D(QQuickScrollBar);
    qreal step = qFuzzyIsNull(d->stepSize) ? 0.1 : d->stepSize;
    bool wasActive = d->active;
    setActive(true);
    setPosition(qMax<qreal>(0.0, d->position - step));
    setActive(wasActive);
}

void QQuickScrollBar::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickScrollBar);
    QQuickControl::mousePressEvent(event);
    d->handleMove(event->localPos());
}

#if QT_CONFIG(quicktemplates2_hover)
void QQuickScrollBar::hoverChange()
{
    Q_D(QQuickScrollBar);
    d->updateActive();
}
#endif

#if QT_CONFIG(accessibility)
void QQuickScrollBar::accessibilityActiveChanged(bool active)
{
    QQuickControl::accessibilityActiveChanged(active);

    Q_D(QQuickScrollBar);
    if (active)
        setAccessibleProperty("pressed", d->pressed);
}

QAccessible::Role QQuickScrollBar::accessibleRole() const
{
    return QAccessible::ScrollBar;
}
#endif

QQuickScrollBarAttachedPrivate::QQuickScrollBarAttachedPrivate()
    : flickable(nullptr),
      horizontal(nullptr),
      vertical(nullptr)
{
}

void QQuickScrollBarAttachedPrivate::setFlickable(QQuickFlickable *item)
{
    if (flickable) {
        // NOTE: Use removeItemChangeListener(Geometry) instead of updateOrRemoveGeometryChangeListener(Size).
        // The latter doesn't remove the listener but only resets its types. Thus, it leaves behind a dangling
        // pointer on destruction.
        QQuickItemPrivate::get(flickable)->removeItemChangeListener(this, QQuickItemPrivate::Geometry);
        if (horizontal)
            cleanupHorizontal();
        if (vertical)
            cleanupVertical();
    }

    flickable = item;

    if (item) {
        QQuickItemPrivate::get(item)->updateOrAddGeometryChangeListener(this, QQuickGeometryChange::Size);
        if (horizontal)
            initHorizontal();
        if (vertical)
            initVertical();
    }
}

void QQuickScrollBarAttachedPrivate::initHorizontal()
{
    Q_ASSERT(flickable && horizontal);

    connect(flickable, &QQuickFlickable::movingHorizontallyChanged, this, &QQuickScrollBarAttachedPrivate::activateHorizontal);

    // TODO: export QQuickFlickableVisibleArea
    QObject *area = flickable->property("visibleArea").value<QObject *>();
    QObject::connect(area, SIGNAL(widthRatioChanged(qreal)), horizontal, SLOT(setSize(qreal)));
    QObject::connect(area, SIGNAL(xPositionChanged(qreal)), horizontal, SLOT(setPosition(qreal)));

    // ensure that the ScrollBar is stacked above the Flickable in a ScrollView
    QQuickItem *parent = horizontal->parentItem();
    if (parent && parent == flickable->parentItem())
        horizontal->stackAfter(flickable);

    layoutHorizontal();
    horizontal->setSize(area->property("widthRatio").toReal());
    horizontal->setPosition(area->property("xPosition").toReal());
}

void QQuickScrollBarAttachedPrivate::initVertical()
{
    Q_ASSERT(flickable && vertical);

    connect(flickable, &QQuickFlickable::movingVerticallyChanged, this, &QQuickScrollBarAttachedPrivate::activateVertical);

    // TODO: export QQuickFlickableVisibleArea
    QObject *area = flickable->property("visibleArea").value<QObject *>();
    QObject::connect(area, SIGNAL(heightRatioChanged(qreal)), vertical, SLOT(setSize(qreal)));
    QObject::connect(area, SIGNAL(yPositionChanged(qreal)), vertical, SLOT(setPosition(qreal)));

    // ensure that the ScrollBar is stacked above the Flickable in a ScrollView
    QQuickItem *parent = vertical->parentItem();
    if (parent && parent == flickable->parentItem())
        vertical->stackAfter(flickable);

    layoutVertical();
    vertical->setSize(area->property("heightRatio").toReal());
    vertical->setPosition(area->property("yPosition").toReal());
}

void QQuickScrollBarAttachedPrivate::cleanupHorizontal()
{
    Q_ASSERT(flickable && horizontal);

    disconnect(flickable, &QQuickFlickable::movingHorizontallyChanged, this, &QQuickScrollBarAttachedPrivate::activateHorizontal);

    // TODO: export QQuickFlickableVisibleArea
    QObject *area = flickable->property("visibleArea").value<QObject *>();
    QObject::disconnect(area, SIGNAL(widthRatioChanged(qreal)), horizontal, SLOT(setSize(qreal)));
    QObject::disconnect(area, SIGNAL(xPositionChanged(qreal)), horizontal, SLOT(setPosition(qreal)));
}

void QQuickScrollBarAttachedPrivate::cleanupVertical()
{
    Q_ASSERT(flickable && vertical);

    disconnect(flickable, &QQuickFlickable::movingVerticallyChanged, this, &QQuickScrollBarAttachedPrivate::activateVertical);

    // TODO: export QQuickFlickableVisibleArea
    QObject *area = flickable->property("visibleArea").value<QObject *>();
    QObject::disconnect(area, SIGNAL(heightRatioChanged(qreal)), vertical, SLOT(setSize(qreal)));
    QObject::disconnect(area, SIGNAL(yPositionChanged(qreal)), vertical, SLOT(setPosition(qreal)));
}

void QQuickScrollBarAttachedPrivate::activateHorizontal()
{
    QQuickScrollBarPrivate *p = QQuickScrollBarPrivate::get(horizontal);
    p->moving = flickable->isMovingHorizontally();
    p->updateActive();
}

void QQuickScrollBarAttachedPrivate::activateVertical()
{
    QQuickScrollBarPrivate *p = QQuickScrollBarPrivate::get(vertical);
    p->moving = flickable->isMovingVertically();
    p->updateActive();
}

// TODO: QQuickFlickable::maxXYExtent()
class QQuickFriendlyFlickable : public QQuickFlickable
{
    friend class QQuickScrollBarAttachedPrivate;
};

void QQuickScrollBarAttachedPrivate::scrollHorizontal()
{
    QQuickFriendlyFlickable *f = reinterpret_cast<QQuickFriendlyFlickable *>(flickable);

    const qreal viewwidth = f->width();
    const qreal maxxextent = -f->maxXExtent() + f->minXExtent();
    qreal cx = horizontal->position() * (maxxextent + viewwidth) - f->minXExtent();
    if (!qIsNaN(cx) && !qFuzzyCompare(cx, flickable->contentX()))
        flickable->setContentX(cx);
}

void QQuickScrollBarAttachedPrivate::scrollVertical()
{
    QQuickFriendlyFlickable *f = reinterpret_cast<QQuickFriendlyFlickable *>(flickable);

    const qreal viewheight = f->height();
    const qreal maxyextent = -f->maxYExtent() + f->minYExtent();
    qreal cy = vertical->position() * (maxyextent + viewheight) - f->minYExtent();
    if (!qIsNaN(cy) && !qFuzzyCompare(cy, flickable->contentY()))
        flickable->setContentY(cy);
}

void QQuickScrollBarAttachedPrivate::mirrorVertical()
{
    layoutVertical(true);
}

void QQuickScrollBarAttachedPrivate::layoutHorizontal(bool move)
{
    Q_ASSERT(horizontal && flickable);
    if (horizontal->parentItem() != flickable)
        return;
    horizontal->setWidth(flickable->width());
    if (move)
        horizontal->setY(flickable->height() - horizontal->height());
}

void QQuickScrollBarAttachedPrivate::layoutVertical(bool move)
{
    Q_ASSERT(vertical && flickable);
    if (vertical->parentItem() != flickable)
        return;
    vertical->setHeight(flickable->height());
    if (move)
        vertical->setX(vertical->isMirrored() ? 0 : flickable->width() - vertical->width());
}

void QQuickScrollBarAttachedPrivate::itemGeometryChanged(QQuickItem *item, const QQuickGeometryChange change, const QRectF &diff)
{
    Q_UNUSED(item);
    Q_UNUSED(change);
    if (horizontal && horizontal->height() > 0) {
#ifdef QT_QUICK_NEW_GEOMETRY_CHANGED_HANDLING // TODO: correct/rename diff to oldGeometry
        bool move = qFuzzyIsNull(horizontal->y()) || qFuzzyCompare(horizontal->y(), diff.height() - horizontal->height());
#else
        bool move = qFuzzyIsNull(horizontal->y()) || qFuzzyCompare(horizontal->y(), item->height() - diff.height() - horizontal->height());
#endif
        if (flickable)
            layoutHorizontal(move);
    }
    if (vertical && vertical->width() > 0) {
#ifdef QT_QUICK_NEW_GEOMETRY_CHANGED_HANDLING // TODO: correct/rename diff to oldGeometry
        bool move = qFuzzyIsNull(vertical->x()) || qFuzzyCompare(vertical->x(), diff.width() - vertical->width());
#else
        bool move = qFuzzyIsNull(vertical->x()) || qFuzzyCompare(vertical->x(), item->width() - diff.width() - vertical->width());
#endif
        if (flickable)
            layoutVertical(move);
    }
}

void QQuickScrollBarAttachedPrivate::itemImplicitWidthChanged(QQuickItem *item)
{
    if (item == vertical && flickable)
        layoutVertical(true);
}

void QQuickScrollBarAttachedPrivate::itemImplicitHeightChanged(QQuickItem *item)
{
    if (item == horizontal && flickable)
        layoutHorizontal(true);
}

void QQuickScrollBarAttachedPrivate::itemDestroyed(QQuickItem *item)
{
    if (item == horizontal)
        horizontal = nullptr;
    if (item == vertical)
        vertical = nullptr;
}

QQuickScrollBarAttached::QQuickScrollBarAttached(QObject *parent)
    : QObject(*(new QQuickScrollBarAttachedPrivate), parent)
{
    Q_D(QQuickScrollBarAttached);
    d->setFlickable(qobject_cast<QQuickFlickable *>(parent));

    if (parent && !d->flickable && !qobject_cast<QQuickScrollView *>(parent))
        qmlWarning(parent) << "ScrollBar must be attached to a Flickable or ScrollView";
}

QQuickScrollBarAttached::~QQuickScrollBarAttached()
{
    Q_D(QQuickScrollBarAttached);
    if (d->horizontal) {
        QQuickItemPrivate::get(d->horizontal)->removeItemChangeListener(d, horizontalChangeTypes);
        d->horizontal = nullptr;
    }
    if (d->vertical) {
        QQuickItemPrivate::get(d->vertical)->removeItemChangeListener(d, verticalChangeTypes);
        d->vertical = nullptr;
    }
    d->setFlickable(nullptr);
}

/*!
    \qmlattachedproperty ScrollBar QtQuick.Controls::ScrollBar::horizontal

    This property attaches a horizontal scroll bar to a \l Flickable.

    \code
    Flickable {
        contentWidth: 2000
        ScrollBar.horizontal: ScrollBar { }
    }
    \endcode

    \sa {Attaching ScrollBar to a Flickable}
*/
QQuickScrollBar *QQuickScrollBarAttached::horizontal() const
{
    Q_D(const QQuickScrollBarAttached);
    return d->horizontal;
}

void QQuickScrollBarAttached::setHorizontal(QQuickScrollBar *horizontal)
{
    Q_D(QQuickScrollBarAttached);
    if (d->horizontal == horizontal)
        return;

    if (d->horizontal) {
        QQuickItemPrivate::get(d->horizontal)->removeItemChangeListener(d, horizontalChangeTypes);
        QObjectPrivate::disconnect(d->horizontal, &QQuickScrollBar::positionChanged, d, &QQuickScrollBarAttachedPrivate::scrollHorizontal);

        if (d->flickable)
            d->cleanupHorizontal();
    }

    d->horizontal = horizontal;

    if (horizontal) {
        if (!horizontal->parentItem())
            horizontal->setParentItem(qobject_cast<QQuickItem *>(parent()));
        horizontal->setOrientation(Qt::Horizontal);

        QQuickItemPrivate::get(horizontal)->addItemChangeListener(d, horizontalChangeTypes);
        QObjectPrivate::connect(horizontal, &QQuickScrollBar::positionChanged, d, &QQuickScrollBarAttachedPrivate::scrollHorizontal);

        if (d->flickable)
            d->initHorizontal();
    }
    emit horizontalChanged();
}

/*!
    \qmlattachedproperty ScrollBar QtQuick.Controls::ScrollBar::vertical

    This property attaches a vertical scroll bar to a \l Flickable.

    \code
    Flickable {
        contentHeight: 2000
        ScrollBar.vertical: ScrollBar { }
    }
    \endcode

    \sa {Attaching ScrollBar to a Flickable}
*/
QQuickScrollBar *QQuickScrollBarAttached::vertical() const
{
    Q_D(const QQuickScrollBarAttached);
    return d->vertical;
}

void QQuickScrollBarAttached::setVertical(QQuickScrollBar *vertical)
{
    Q_D(QQuickScrollBarAttached);
    if (d->vertical == vertical)
        return;

    if (d->vertical) {
        QQuickItemPrivate::get(d->vertical)->removeItemChangeListener(d, verticalChangeTypes);
        QObjectPrivate::disconnect(d->vertical, &QQuickScrollBar::mirroredChanged, d, &QQuickScrollBarAttachedPrivate::mirrorVertical);
        QObjectPrivate::disconnect(d->vertical, &QQuickScrollBar::positionChanged, d, &QQuickScrollBarAttachedPrivate::scrollVertical);

        if (d->flickable)
            d->cleanupVertical();
    }

    d->vertical = vertical;

    if (vertical) {
        if (!vertical->parentItem())
            vertical->setParentItem(qobject_cast<QQuickItem *>(parent()));
        vertical->setOrientation(Qt::Vertical);

        QQuickItemPrivate::get(vertical)->addItemChangeListener(d, verticalChangeTypes);
        QObjectPrivate::connect(vertical, &QQuickScrollBar::mirroredChanged, d, &QQuickScrollBarAttachedPrivate::mirrorVertical);
        QObjectPrivate::connect(vertical, &QQuickScrollBar::positionChanged, d, &QQuickScrollBarAttachedPrivate::scrollVertical);

        if (d->flickable)
            d->initVertical();
    }
    emit verticalChanged();
}

QT_END_NAMESPACE
