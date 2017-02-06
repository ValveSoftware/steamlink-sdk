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

#include "qquickflickable_p.h"
#include "qquickflickable_p_p.h"
#include "qquickflickablebehavior_p.h"
#include "qquickwindow.h"
#include "qquickwindow_p.h"
#include "qquickevents_p_p.h"

#include <QtQuick/private/qquicktransition_p.h>
#include <private/qqmlglobal_p.h>

#include <QtQml/qqmlinfo.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qstylehints.h>
#include <QtCore/qmath.h>
#include "qplatformdefs.h"

#include <cmath>

QT_BEGIN_NAMESPACE

// FlickThreshold determines how far the "mouse" must have moved
// before we perform a flick.
static const int FlickThreshold = 15;

// RetainGrabVelocity is the maxmimum instantaneous velocity that
// will ensure the Flickable retains the grab on consecutive flicks.
static const int RetainGrabVelocity = 100;

#ifdef Q_OS_OSX
static const int MovementEndingTimerInterval = 100;
#endif

// Currently std::round can't be used on Android when using ndk g++, so
// use C version instead. We could just define two versions of Round, one
// for float and one for double, but then only one of them would be used
// and compiler would trigger a warning about unused function.
//
// See https://code.google.com/p/android/issues/detail?id=54418
template<typename T>
static T Round(T t) {
    return round(t);
}
template<>
Q_DECL_UNUSED float Round<float>(float f) {
    return roundf(f);
}

static qreal EaseOvershoot(qreal t) {
    return qAtan(t);
}

QQuickFlickableVisibleArea::QQuickFlickableVisibleArea(QQuickFlickable *parent)
    : QObject(parent), flickable(parent), m_xPosition(0.), m_widthRatio(0.)
    , m_yPosition(0.), m_heightRatio(0.)
{
}

qreal QQuickFlickableVisibleArea::widthRatio() const
{
    return m_widthRatio;
}

qreal QQuickFlickableVisibleArea::xPosition() const
{
    return m_xPosition;
}

qreal QQuickFlickableVisibleArea::heightRatio() const
{
    return m_heightRatio;
}

qreal QQuickFlickableVisibleArea::yPosition() const
{
    return m_yPosition;
}

void QQuickFlickableVisibleArea::updateVisible()
{
    QQuickFlickablePrivate *p = QQuickFlickablePrivate::get(flickable);

    bool changeX = false;
    bool changeY = false;
    bool changeWidth = false;
    bool changeHeight = false;

    // Vertical
    const qreal viewheight = flickable->height();
    const qreal maxyextent = -flickable->maxYExtent() + flickable->minYExtent();
    qreal pagePos = (-p->vData.move.value() + flickable->minYExtent()) / (maxyextent + viewheight);
    qreal pageSize = viewheight / (maxyextent + viewheight);

    if (pageSize != m_heightRatio) {
        m_heightRatio = pageSize;
        changeHeight = true;
    }
    if (pagePos != m_yPosition) {
        m_yPosition = pagePos;
        changeY = true;
    }

    // Horizontal
    const qreal viewwidth = flickable->width();
    const qreal maxxextent = -flickable->maxXExtent() + flickable->minXExtent();
    pagePos = (-p->hData.move.value() + flickable->minXExtent()) / (maxxextent + viewwidth);
    pageSize = viewwidth / (maxxextent + viewwidth);

    if (pageSize != m_widthRatio) {
        m_widthRatio = pageSize;
        changeWidth = true;
    }
    if (pagePos != m_xPosition) {
        m_xPosition = pagePos;
        changeX = true;
    }

    if (changeX)
        emit xPositionChanged(m_xPosition);
    if (changeY)
        emit yPositionChanged(m_yPosition);
    if (changeWidth)
        emit widthRatioChanged(m_widthRatio);
    if (changeHeight)
        emit heightRatioChanged(m_heightRatio);
}


class QQuickFlickableReboundTransition : public QQuickTransitionManager
{
public:
    QQuickFlickableReboundTransition(QQuickFlickable *f, const QString &name)
        : flickable(f), axisData(0), propName(name), active(false)
    {
    }

    ~QQuickFlickableReboundTransition()
    {
        flickable = 0;
    }

    bool startTransition(QQuickFlickablePrivate::AxisData *data, qreal toPos) {
        QQuickFlickablePrivate *fp = QQuickFlickablePrivate::get(flickable);
        if (!fp->rebound || !fp->rebound->enabled())
            return false;
        active = true;
        axisData = data;
        axisData->transitionTo = toPos;
        axisData->transitionToSet = true;

        actions.clear();
        actions << QQuickStateAction(fp->contentItem, propName, toPos);
        QQuickTransitionManager::transition(actions, fp->rebound, fp->contentItem);
        return true;
    }

    bool isActive() const {
        return active;
    }

    void stopTransition() {
        if (!flickable || !isRunning())
            return;
        QQuickFlickablePrivate *fp = QQuickFlickablePrivate::get(flickable);
        if (axisData == &fp->hData)
            axisData->move.setValue(-flickable->contentX());
        else
            axisData->move.setValue(-flickable->contentY());
        cancel();
        active = false;
    }

protected:
    void finished() Q_DECL_OVERRIDE {
        if (!flickable)
            return;
        axisData->move.setValue(axisData->transitionTo);
        QQuickFlickablePrivate *fp = QQuickFlickablePrivate::get(flickable);
        active = false;

        if (!fp->hData.transitionToBounds->isActive()
                && !fp->vData.transitionToBounds->isActive()) {
            flickable->movementEnding();
        }
    }

private:
    QQuickStateOperation::ActionList actions;
    QQuickFlickable *flickable;
    QQuickFlickablePrivate::AxisData *axisData;
    QString propName;
    bool active;
};

QQuickFlickablePrivate::AxisData::~AxisData()
{
    delete transitionToBounds;
}


QQuickFlickablePrivate::QQuickFlickablePrivate()
  : contentItem(new QQuickItem)
    , hData(this, &QQuickFlickablePrivate::setViewportX)
    , vData(this, &QQuickFlickablePrivate::setViewportY)
    , hMoved(false), vMoved(false)
    , stealMouse(false), pressed(false)
    , scrollingPhase(false), interactive(true), calcVelocity(false)
    , pixelAligned(false)
    , lastPosTime(-1)
    , lastPressTime(0)
    , deceleration(QML_FLICK_DEFAULTDECELERATION)
    , maxVelocity(QML_FLICK_DEFAULTMAXVELOCITY), reportedVelocitySmoothing(100)
    , delayedPressEvent(0), pressDelay(0), fixupDuration(400)
    , flickBoost(1.0), fixupMode(Normal), vTime(0), visibleArea(0)
    , flickableDirection(QQuickFlickable::AutoFlickDirection)
    , boundsBehavior(QQuickFlickable::DragAndOvershootBounds)
    , rebound(0)
{
}

void QQuickFlickablePrivate::init()
{
    Q_Q(QQuickFlickable);
    QQml_setParent_noEvent(contentItem, q);
    contentItem->setParentItem(q);
    qmlobject_connect(&timeline, QQuickTimeLine, SIGNAL(completed()),
                      q, QQuickFlickable, SLOT(timelineCompleted()))
    qmlobject_connect(&velocityTimeline, QQuickTimeLine, SIGNAL(completed()),
                      q, QQuickFlickable, SLOT(velocityTimelineCompleted()))
    q->setAcceptedMouseButtons(Qt::LeftButton);
    q->setFiltersChildMouseEvents(true);
    QQuickItemPrivate *viewportPrivate = QQuickItemPrivate::get(contentItem);
    viewportPrivate->addItemChangeListener(this, QQuickItemPrivate::Geometry);
}

/*
    Returns the amount to overshoot by given a velocity.
    Will be roughly in range 0 - size/4
*/
qreal QQuickFlickablePrivate::overShootDistance(qreal size)
{
    if (maxVelocity <= 0)
        return 0.0;

    return qMin(qreal(QML_FLICK_OVERSHOOT), size/3);
}

void QQuickFlickablePrivate::AxisData::addVelocitySample(qreal v, qreal maxVelocity)
{
    if (v > maxVelocity)
        v = maxVelocity;
    else if (v < -maxVelocity)
        v = -maxVelocity;
    velocityBuffer.append(v);
    if (velocityBuffer.count() > QML_FLICK_SAMPLEBUFFER)
        velocityBuffer.remove(0);
}

void QQuickFlickablePrivate::AxisData::updateVelocity()
{
    velocity = 0;
    if (velocityBuffer.count() > QML_FLICK_DISCARDSAMPLES) {
        int count = velocityBuffer.count()-QML_FLICK_DISCARDSAMPLES;
        for (int i = 0; i < count; ++i) {
            qreal v = velocityBuffer.at(i);
            velocity += v;
        }
        velocity /= count;
    }
}

void QQuickFlickablePrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &)
{
    Q_Q(QQuickFlickable);
    if (item == contentItem) {
        Qt::Orientations orient = 0;
        if (change.xChange())
            orient |= Qt::Horizontal;
        if (change.yChange())
            orient |= Qt::Vertical;
        if (orient)
            q->viewportMoved(orient);
        if (orient & Qt::Horizontal)
            emit q->contentXChanged();
        if (orient & Qt::Vertical)
            emit q->contentYChanged();
    }
}

bool QQuickFlickablePrivate::flickX(qreal velocity)
{
    Q_Q(QQuickFlickable);
    return flick(hData, q->minXExtent(), q->maxXExtent(), q->width(), fixupX_callback, velocity);
}

bool QQuickFlickablePrivate::flickY(qreal velocity)
{
    Q_Q(QQuickFlickable);
    return flick(vData, q->minYExtent(), q->maxYExtent(), q->height(), fixupY_callback, velocity);
}

bool QQuickFlickablePrivate::flick(AxisData &data, qreal minExtent, qreal maxExtent, qreal,
                                         QQuickTimeLineCallback::Callback fixupCallback, qreal velocity)
{
    Q_Q(QQuickFlickable);
    qreal maxDistance = -1;
    data.fixingUp = false;
    // -ve velocity means list is moving up
    if (velocity > 0) {
        maxDistance = qAbs(minExtent - data.move.value());
        data.flickTarget = minExtent;
    } else {
        maxDistance = qAbs(maxExtent - data.move.value());
        data.flickTarget = maxExtent;
    }
    if (maxDistance > 0 || boundsBehavior & QQuickFlickable::OvershootBounds) {
        qreal v = velocity;
        if (maxVelocity != -1 && maxVelocity < qAbs(v)) {
            if (v < 0)
                v = -maxVelocity;
            else
                v = maxVelocity;
        }

        // adjust accel so that we hit a full pixel
        qreal accel = deceleration;
        qreal v2 = v * v;
        qreal dist = v2 / (accel * 2.0);
        if (v > 0)
            dist = -dist;
        qreal target = -Round(-(data.move.value() - dist));
        dist = -target + data.move.value();
        accel = v2 / (2.0f * qAbs(dist));

        resetTimeline(data);
        if (!data.inOvershoot) {
            if (boundsBehavior & QQuickFlickable::OvershootBounds)
                timeline.accel(data.move, v, accel);
            else
                timeline.accel(data.move, v, accel, maxDistance);
        }
        timeline.callback(QQuickTimeLineCallback(&data.move, fixupCallback, this));

        if (&data == &hData)
            return !hData.flicking && q->xflick();
        else if (&data == &vData)
            return !vData.flicking && q->yflick();
        return false;
    } else {
        resetTimeline(data);
        fixup(data, minExtent, maxExtent);
        return false;
    }
}

void QQuickFlickablePrivate::fixupY_callback(void *data)
{
    ((QQuickFlickablePrivate *)data)->fixupY();
}

void QQuickFlickablePrivate::fixupX_callback(void *data)
{
    ((QQuickFlickablePrivate *)data)->fixupX();
}

void QQuickFlickablePrivate::fixupX()
{
    Q_Q(QQuickFlickable);
    if (!q->isComponentComplete())
        return; //Do not fixup from initialization values
    fixup(hData, q->minXExtent(), q->maxXExtent());
}

void QQuickFlickablePrivate::fixupY()
{
    Q_Q(QQuickFlickable);
    if (!q->isComponentComplete())
        return; //Do not fixup from initialization values
    fixup(vData, q->minYExtent(), q->maxYExtent());
}

void QQuickFlickablePrivate::adjustContentPos(AxisData &data, qreal toPos)
{
    Q_Q(QQuickFlickable);
    switch (fixupMode) {
    case Immediate:
        timeline.set(data.move, toPos);
        break;
    case ExtentChanged:
        // The target has changed. Don't start from the beginning; just complete the
        // second half of the animation using the new extent.
        timeline.move(data.move, toPos, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
        data.fixingUp = true;
        break;
    default: {
            if (data.transitionToBounds && data.transitionToBounds->startTransition(&data, toPos)) {
                q->movementStarting();
                data.fixingUp = true;
            } else {
                qreal dist = toPos - data.move;
                timeline.move(data.move, toPos - dist/2, QEasingCurve(QEasingCurve::InQuad), fixupDuration/4);
                timeline.move(data.move, toPos, QEasingCurve(QEasingCurve::OutExpo), 3*fixupDuration/4);
                data.fixingUp = true;
            }
        }
    }
}

void QQuickFlickablePrivate::resetTimeline(AxisData &data)
{
    timeline.reset(data.move);
    if (data.transitionToBounds)
        data.transitionToBounds->stopTransition();
}

void QQuickFlickablePrivate::clearTimeline()
{
    timeline.clear();
    if (hData.transitionToBounds)
        hData.transitionToBounds->stopTransition();
    if (vData.transitionToBounds)
        vData.transitionToBounds->stopTransition();
}

void QQuickFlickablePrivate::fixup(AxisData &data, qreal minExtent, qreal maxExtent)
{
    if (data.move.value() >= minExtent || maxExtent > minExtent) {
        resetTimeline(data);
        if (data.move.value() != minExtent) {
            adjustContentPos(data, minExtent);
        }
    } else if (data.move.value() <= maxExtent) {
        resetTimeline(data);
        adjustContentPos(data, maxExtent);
    } else if (-Round(-data.move.value()) != data.move.value()) {
        // We could animate, but since it is less than 0.5 pixel it's probably not worthwhile.
        resetTimeline(data);
        qreal val = data.move.value();
        if (std::abs(-Round(-val) - val) < 0.25) // round small differences
            val = -Round(-val);
        else if (data.smoothVelocity.value() > 0) // continue direction of motion for larger
            val = -std::floor(-val);
        else if (data.smoothVelocity.value() < 0)
            val = -std::ceil(-val);
        else // otherwise round
            val = -Round(-val);
        timeline.set(data.move, val);
    }
    data.inOvershoot = false;
    fixupMode = Normal;
    data.vTime = timeline.time();
}

static bool fuzzyLessThanOrEqualTo(qreal a, qreal b)
{
    if (a == 0.0 || b == 0.0) {
        // qFuzzyCompare is broken
        a += 1.0;
        b += 1.0;
    }
    return a <= b || qFuzzyCompare(a, b);
}

void QQuickFlickablePrivate::updateBeginningEnd()
{
    Q_Q(QQuickFlickable);
    bool atBoundaryChange = false;

    // Vertical
    const qreal maxyextent = -q->maxYExtent();
    const qreal minyextent = -q->minYExtent();
    const qreal ypos = -vData.move.value();
    bool atBeginning = fuzzyLessThanOrEqualTo(ypos, minyextent);
    bool atEnd = fuzzyLessThanOrEqualTo(maxyextent, ypos);

    if (atBeginning != vData.atBeginning) {
        vData.atBeginning = atBeginning;
        atBoundaryChange = true;
    }
    if (atEnd != vData.atEnd) {
        vData.atEnd = atEnd;
        atBoundaryChange = true;
    }

    // Horizontal
    const qreal maxxextent = -q->maxXExtent();
    const qreal minxextent = -q->minXExtent();
    const qreal xpos = -hData.move.value();
    atBeginning = fuzzyLessThanOrEqualTo(xpos, minxextent);
    atEnd = fuzzyLessThanOrEqualTo(maxxextent, xpos);

    if (atBeginning != hData.atBeginning) {
        hData.atBeginning = atBeginning;
        atBoundaryChange = true;
    }
    if (atEnd != hData.atEnd) {
        hData.atEnd = atEnd;
        atBoundaryChange = true;
    }

    if (vData.extentsChanged) {
        vData.extentsChanged = false;
        qreal originY = q->originY();
        if (vData.origin != originY) {
            vData.origin = originY;
            emit q->originYChanged();
        }
    }

    if (hData.extentsChanged) {
        hData.extentsChanged = false;
        qreal originX = q->originX();
        if (hData.origin != originX) {
            hData.origin = originX;
            emit q->originXChanged();
        }
    }

    if (atBoundaryChange)
        emit q->isAtBoundaryChanged();

    if (visibleArea)
        visibleArea->updateVisible();
}

/*
XXXTODO add docs describing moving, dragging, flicking properties, e.g.

When the user starts dragging the Flickable, the dragging and moving properties
will be true.

If the velocity is sufficient when the drag is ended, flicking may begin.

The moving properties will remain true until all dragging and flicking
is finished.
*/

/*!
    \qmlsignal QtQuick::Flickable::dragStarted()

    This signal is emitted when the view starts to be dragged due to user
    interaction.

    The corresponding handler is \c onDragStarted.
*/

/*!
    \qmlsignal QtQuick::Flickable::dragEnded()

    This signal is emitted when the user stops dragging the view.

    If the velocity of the drag is sufficient at the time the
    touch/mouse button is released then a flick will start.

    The corresponding handler is \c onDragEnded.
*/

/*!
    \qmltype Flickable
    \instantiates QQuickFlickable
    \inqmlmodule QtQuick
    \ingroup qtquick-input
    \ingroup qtquick-containers

    \brief Provides a surface that can be "flicked"
    \inherits Item

    The Flickable item places its children on a surface that can be dragged
    and flicked, causing the view onto the child items to scroll. This
    behavior forms the basis of Items that are designed to show large numbers
    of child items, such as \l ListView and \l GridView.

    In traditional user interfaces, views can be scrolled using standard
    controls, such as scroll bars and arrow buttons. In some situations, it
    is also possible to drag the view directly by pressing and holding a
    mouse button while moving the cursor. In touch-based user interfaces,
    this dragging action is often complemented with a flicking action, where
    scrolling continues after the user has stopped touching the view.

    Flickable does not automatically clip its contents. If it is not used as
    a full-screen item, you should consider setting the \l{Item::}{clip} property
    to true.

    \section1 Example Usage

    \div {class="float-right"}
    \inlineimage flickable.gif
    \enddiv

    The following example shows a small view onto a large image in which the
    user can drag or flick the image in order to view different parts of it.

    \snippet qml/flickable.qml document

    \clearfloat

    Items declared as children of a Flickable are automatically parented to the
    Flickable's \l contentItem.  This should be taken into account when
    operating on the children of the Flickable; it is usually the children of
    \c contentItem that are relevant.  For example, the bound of Items added
    to the Flickable will be available by \c contentItem.childrenRect

    \section1 Limitations

    \note Due to an implementation detail, items placed inside a Flickable cannot anchor to it by
    \c id. Use \c parent instead.
*/

/*!
    \qmlsignal QtQuick::Flickable::movementStarted()

    This signal is emitted when the view begins moving due to user
    interaction or a generated flick().

    The corresponding handler is \c onMovementStarted.
*/

/*!
    \qmlsignal QtQuick::Flickable::movementEnded()

    This signal is emitted when the view stops moving due to user
    interaction or a generated flick().  If a flick was active, this signal will
    be emitted once the flick stops.  If a flick was not
    active, this signal will be emitted when the
    user stops dragging - i.e. a mouse or touch release.

    The corresponding handler is \c onMovementEnded.
*/

/*!
    \qmlsignal QtQuick::Flickable::flickStarted()

    This signal is emitted when the view is flicked.  A flick
    starts from the point that the mouse or touch is released,
    while still in motion.

    The corresponding handler is \c onFlickStarted.
*/

/*!
    \qmlsignal QtQuick::Flickable::flickEnded()

    This signal is emitted when the view stops moving due to a flick.

    The corresponding handler is \c onFlickEnded.
*/

/*!
    \qmlpropertygroup QtQuick::Flickable::visibleArea
    \qmlproperty real QtQuick::Flickable::visibleArea.xPosition
    \qmlproperty real QtQuick::Flickable::visibleArea.widthRatio
    \qmlproperty real QtQuick::Flickable::visibleArea.yPosition
    \qmlproperty real QtQuick::Flickable::visibleArea.heightRatio

    These properties describe the position and size of the currently viewed area.
    The size is defined as the percentage of the full view currently visible,
    scaled to 0.0 - 1.0.  The page position is usually in the range 0.0 (beginning) to
    1.0 minus size ratio (end), i.e. \c yPosition is in the range 0.0 to 1.0-\c heightRatio.
    However, it is possible for the contents to be dragged outside of the normal
    range, resulting in the page positions also being outside the normal range.

    These properties are typically used to draw a scrollbar. For example:

    \snippet qml/flickableScrollbar.qml 0
    \dots 8
    \snippet qml/flickableScrollbar.qml 1

    \sa {customitems/scrollbar}{UI Components: Scrollbar Example}
*/
QQuickFlickable::QQuickFlickable(QQuickItem *parent)
  : QQuickItem(*(new QQuickFlickablePrivate), parent)
{
    Q_D(QQuickFlickable);
    d->init();
}

QQuickFlickable::QQuickFlickable(QQuickFlickablePrivate &dd, QQuickItem *parent)
  : QQuickItem(dd, parent)
{
    Q_D(QQuickFlickable);
    d->init();
}

QQuickFlickable::~QQuickFlickable()
{
}

/*!
    \qmlproperty real QtQuick::Flickable::contentX
    \qmlproperty real QtQuick::Flickable::contentY

    These properties hold the surface coordinate currently at the top-left
    corner of the Flickable. For example, if you flick an image up 100 pixels,
    \c contentY will be 100.
*/
qreal QQuickFlickable::contentX() const
{
    Q_D(const QQuickFlickable);
    return -d->contentItem->x();
}

void QQuickFlickable::setContentX(qreal pos)
{
    Q_D(QQuickFlickable);
    d->hData.explicitValue = true;
    d->resetTimeline(d->hData);
    d->hData.vTime = d->timeline.time();
    movementEnding(true, false);
    if (-pos != d->hData.move.value())
        d->hData.move.setValue(-pos);
}

qreal QQuickFlickable::contentY() const
{
    Q_D(const QQuickFlickable);
    return -d->contentItem->y();
}

void QQuickFlickable::setContentY(qreal pos)
{
    Q_D(QQuickFlickable);
    d->vData.explicitValue = true;
    d->resetTimeline(d->vData);
    d->vData.vTime = d->timeline.time();
    movementEnding(false, true);
    if (-pos != d->vData.move.value())
        d->vData.move.setValue(-pos);
}

/*!
    \qmlproperty bool QtQuick::Flickable::interactive

    This property describes whether the user can interact with the Flickable.
    A user cannot drag or flick a Flickable that is not interactive.

    By default, this property is true.

    This property is useful for temporarily disabling flicking. This allows
    special interaction with Flickable's children; for example, you might want
    to freeze a flickable map while scrolling through a pop-up dialog that
    is a child of the Flickable.
*/
bool QQuickFlickable::isInteractive() const
{
    Q_D(const QQuickFlickable);
    return d->interactive;
}

void QQuickFlickable::setInteractive(bool interactive)
{
    Q_D(QQuickFlickable);
    if (interactive != d->interactive) {
        d->interactive = interactive;
        if (!interactive) {
            d->cancelInteraction();
        }
        emit interactiveChanged();
    }
}

/*!
    \qmlproperty real QtQuick::Flickable::horizontalVelocity
    \qmlproperty real QtQuick::Flickable::verticalVelocity

    The instantaneous velocity of movement along the x and y axes, in pixels/sec.

    The reported velocity is smoothed to avoid erratic output.

    Note that for views with a large content size (more than 10 times the view size),
    the velocity of the flick may exceed the velocity of the touch in the case
    of multiple quick consecutive flicks.  This allows the user to flick faster
    through large content.
*/
qreal QQuickFlickable::horizontalVelocity() const
{
    Q_D(const QQuickFlickable);
    return d->hData.smoothVelocity.value();
}

qreal QQuickFlickable::verticalVelocity() const
{
    Q_D(const QQuickFlickable);
    return d->vData.smoothVelocity.value();
}

/*!
    \qmlproperty bool QtQuick::Flickable::atXBeginning
    \qmlproperty bool QtQuick::Flickable::atXEnd
    \qmlproperty bool QtQuick::Flickable::atYBeginning
    \qmlproperty bool QtQuick::Flickable::atYEnd

    These properties are true if the flickable view is positioned at the beginning,
    or end respectively.
*/
bool QQuickFlickable::isAtXEnd() const
{
    Q_D(const QQuickFlickable);
    return d->hData.atEnd;
}

bool QQuickFlickable::isAtXBeginning() const
{
    Q_D(const QQuickFlickable);
    return d->hData.atBeginning;
}

bool QQuickFlickable::isAtYEnd() const
{
    Q_D(const QQuickFlickable);
    return d->vData.atEnd;
}

bool QQuickFlickable::isAtYBeginning() const
{
    Q_D(const QQuickFlickable);
    return d->vData.atBeginning;
}

/*!
    \qmlproperty Item QtQuick::Flickable::contentItem

    The internal item that contains the Items to be moved in the Flickable.

    Items declared as children of a Flickable are automatically parented to the Flickable's contentItem.

    Items created dynamically need to be explicitly parented to the \e contentItem:
    \code
    Flickable {
        id: myFlickable
        function addItem(file) {
            var component = Qt.createComponent(file)
            component.createObject(myFlickable.contentItem);
        }
    }
    \endcode
*/
QQuickItem *QQuickFlickable::contentItem()
{
    Q_D(QQuickFlickable);
    return d->contentItem;
}

QQuickFlickableVisibleArea *QQuickFlickable::visibleArea()
{
    Q_D(QQuickFlickable);
    if (!d->visibleArea) {
        d->visibleArea = new QQuickFlickableVisibleArea(this);
        d->visibleArea->updateVisible(); // calculate initial ratios
    }
    return d->visibleArea;
}

/*!
    \qmlproperty enumeration QtQuick::Flickable::flickableDirection

    This property determines which directions the view can be flicked.

    \list
    \li Flickable.AutoFlickDirection (default) - allows flicking vertically if the
    \e contentHeight is not equal to the \e height of the Flickable.
    Allows flicking horizontally if the \e contentWidth is not equal
    to the \e width of the Flickable.
    \li Flickable.AutoFlickIfNeeded - allows flicking vertically if the
    \e contentHeight is greater than the \e height of the Flickable.
    Allows flicking horizontally if the \e contentWidth is greater than
    to the \e width of the Flickable. (since \c{QtQuick 2.7})
    \li Flickable.HorizontalFlick - allows flicking horizontally.
    \li Flickable.VerticalFlick - allows flicking vertically.
    \li Flickable.HorizontalAndVerticalFlick - allows flicking in both directions.
    \endlist
*/
QQuickFlickable::FlickableDirection QQuickFlickable::flickableDirection() const
{
    Q_D(const QQuickFlickable);
    return d->flickableDirection;
}

void QQuickFlickable::setFlickableDirection(FlickableDirection direction)
{
    Q_D(QQuickFlickable);
    if (direction != d->flickableDirection) {
        d->flickableDirection = direction;
        emit flickableDirectionChanged();
    }
}

/*!
    \qmlproperty bool QtQuick::Flickable::pixelAligned

    This property sets the alignment of \l contentX and \l contentY to
    pixels (\c true) or subpixels (\c false).

    Enable pixelAligned to optimize for still content or moving content with
    high constrast edges, such as one-pixel-wide lines, text or vector graphics.
    Disable pixelAligned when optimizing for animation quality.

    The default is \c false.
*/
bool QQuickFlickable::pixelAligned() const
{
    Q_D(const QQuickFlickable);
    return d->pixelAligned;
}

void QQuickFlickable::setPixelAligned(bool align)
{
    Q_D(QQuickFlickable);
    if (align != d->pixelAligned) {
        d->pixelAligned = align;
        emit pixelAlignedChanged();
    }
}

qint64 QQuickFlickablePrivate::computeCurrentTime(QInputEvent *event)
{
    if (0 != event->timestamp())
        return event->timestamp();
    if (!timer.isValid())
        return 0LL;
    return timer.elapsed();
}

qreal QQuickFlickablePrivate::devicePixelRatio()
{
    return (window ? window->effectiveDevicePixelRatio() : qApp->devicePixelRatio());
}

void QQuickFlickablePrivate::handleMousePressEvent(QMouseEvent *event)
{
    Q_Q(QQuickFlickable);
    timer.start();
    if (interactive && timeline.isActive()
        && ((qAbs(hData.smoothVelocity.value()) > RetainGrabVelocity && !hData.fixingUp && !hData.inOvershoot)
            || (qAbs(vData.smoothVelocity.value()) > RetainGrabVelocity && !vData.fixingUp && !vData.inOvershoot))) {
        stealMouse = true; // If we've been flicked then steal the click.
        int flickTime = timeline.time();
        if (flickTime > 600) {
            // too long between flicks - cancel boost
            hData.continuousFlickVelocity = 0;
            vData.continuousFlickVelocity = 0;
            flickBoost = 1.0;
        } else {
            hData.continuousFlickVelocity = -hData.smoothVelocity.value();
            vData.continuousFlickVelocity = -vData.smoothVelocity.value();
            if (flickTime > 300) // slower flicking - reduce boost
                flickBoost = qMax(1.0, flickBoost - 0.5);
        }
    } else {
        stealMouse = false;
        hData.continuousFlickVelocity = 0;
        vData.continuousFlickVelocity = 0;
        flickBoost = 1.0;
    }
    q->setKeepMouseGrab(stealMouse);

    maybeBeginDrag(computeCurrentTime(event), event->localPos());
}

void QQuickFlickablePrivate::maybeBeginDrag(qint64 currentTimestamp, const QPointF &pressPosn)
{
    Q_Q(QQuickFlickable);
    clearDelayedPress();
    pressed = true;

    if (hData.transitionToBounds)
        hData.transitionToBounds->stopTransition();
    if (vData.transitionToBounds)
        vData.transitionToBounds->stopTransition();
    if (!hData.fixingUp)
        resetTimeline(hData);
    if (!vData.fixingUp)
        resetTimeline(vData);

    hData.reset();
    vData.reset();
    hData.dragMinBound = q->minXExtent() - hData.startMargin;
    vData.dragMinBound = q->minYExtent() - vData.startMargin;
    hData.dragMaxBound = q->maxXExtent() + hData.endMargin;
    vData.dragMaxBound = q->maxYExtent() + vData.endMargin;
    fixupMode = Normal;
    lastPos = QPointF();
    pressPos = pressPosn;
    hData.pressPos = hData.move.value();
    vData.pressPos = vData.move.value();
    bool wasFlicking = hData.flicking || vData.flicking;
    if (hData.flicking) {
        hData.flicking = false;
        emit q->flickingHorizontallyChanged();
    }
    if (vData.flicking) {
        vData.flicking = false;
        emit q->flickingVerticallyChanged();
    }
    if (wasFlicking)
        emit q->flickingChanged();
    lastPosTime = lastPressTime = currentTimestamp;
    vData.velocityTime.start();
    hData.velocityTime.start();
}

void QQuickFlickablePrivate::drag(qint64 currentTimestamp, QEvent::Type eventType, const QPointF &localPos,
                                  const QVector2D &deltas, bool overThreshold, bool momentum,
                                  bool velocitySensitiveOverBounds, const QVector2D &velocity)
{
    Q_Q(QQuickFlickable);
    bool rejectY = false;
    bool rejectX = false;

    bool keepY = q->yflick();
    bool keepX = q->xflick();

    bool stealY = false;
    bool stealX = false;
    if (eventType == QEvent::MouseMove) {
        stealX = stealY = stealMouse;
    } else if (eventType == QEvent::Wheel) {
        stealX = stealY = scrollingPhase;
    }

    bool prevHMoved = hMoved;
    bool prevVMoved = vMoved;

    qint64 elapsedSincePress = currentTimestamp - lastPressTime;

    if (q->yflick()) {
        qreal dy = deltas.y();
        if (overThreshold || elapsedSincePress > 200) {
            if (!vMoved)
                vData.dragStartOffset = dy;
            qreal newY = dy + vData.pressPos - vData.dragStartOffset;
            // Recalculate bounds in case margins have changed, but use the content
            // size estimate taken at the start of the drag in case the drag causes
            // the estimate to be altered
            const qreal minY = vData.dragMinBound + vData.startMargin;
            const qreal maxY = vData.dragMaxBound - vData.endMargin;
            if (!(boundsBehavior & QQuickFlickable::DragOverBounds)) {
                if (fuzzyLessThanOrEqualTo(newY, maxY)) {
                    newY = maxY;
                    rejectY = vData.pressPos == maxY && vData.move.value() == maxY && dy < 0;
                }
                if (fuzzyLessThanOrEqualTo(minY, newY)) {
                    newY = minY;
                    rejectY |= vData.pressPos == minY && vData.move.value() == minY && dy > 0;
                }
            } else {
                qreal vel = velocity.y() / QML_FLICK_OVERSHOOTFRICTION;
                if (vel > 0. && vel > vData.velocity)
                    vData.velocity = qMin(velocity.y() / QML_FLICK_OVERSHOOTFRICTION, float(QML_FLICK_DEFAULTMAXVELOCITY));
                else if (vel < 0. && vel < vData.velocity)
                    vData.velocity = qMax(velocity.y() / QML_FLICK_OVERSHOOTFRICTION, -float(QML_FLICK_DEFAULTMAXVELOCITY));
                if (newY > minY) {
                    // Overshoot beyond the top.  But don't wait for momentum phase to end before returning to bounds.
                    if (momentum && vData.atBeginning) {
                        if (!vData.inRebound) {
                            vData.inRebound = true;
                            q->returnToBounds();
                        }
                        return;
                    }
                    if (velocitySensitiveOverBounds) {
                        qreal overshoot = (newY - minY) * vData.velocity / QML_FLICK_DEFAULTMAXVELOCITY / QML_FLICK_OVERSHOOTFRICTION;
                        overshoot = QML_FLICK_OVERSHOOT * devicePixelRatio() * EaseOvershoot(overshoot / QML_FLICK_OVERSHOOT / devicePixelRatio());
                        newY = minY + overshoot;
                    } else {
                        newY = minY + (newY - minY) / 2;
                    }
                } else if (newY < maxY && maxY - minY <= 0) {
                    // Overshoot beyond the bottom.  But don't wait for momentum phase to end before returning to bounds.
                    if (momentum && vData.atEnd) {
                        if (!vData.inRebound) {
                            vData.inRebound = true;
                            q->returnToBounds();
                        }
                        return;
                    }
                    if (velocitySensitiveOverBounds) {
                        qreal overshoot = (newY - maxY) * vData.velocity / QML_FLICK_DEFAULTMAXVELOCITY / QML_FLICK_OVERSHOOTFRICTION;
                        overshoot = QML_FLICK_OVERSHOOT * devicePixelRatio() * EaseOvershoot(overshoot / QML_FLICK_OVERSHOOT / devicePixelRatio());
                        newY = maxY - overshoot;
                    } else {
                        newY = maxY + (newY - maxY) / 2;
                    }
                }
            }
            if (!rejectY && stealMouse && dy != 0.0 && dy != vData.previousDragDelta) {
                clearTimeline();
                vData.move.setValue(newY);
                vMoved = true;
            }
            if (!rejectY && overThreshold)
                stealY = true;

            if ((newY >= minY && vData.pressPos == minY && vData.move.value() == minY && dy > 0)
                        || (newY <= maxY && vData.pressPos == maxY && vData.move.value() == maxY && dy < 0)) {
                keepY = false;
            }
        }
        vData.previousDragDelta = dy;
    }

    if (q->xflick()) {
        qreal dx = deltas.x();
        if (overThreshold || elapsedSincePress > 200) {
            if (!hMoved)
                hData.dragStartOffset = dx;
            qreal newX = dx + hData.pressPos - hData.dragStartOffset;
            const qreal minX = hData.dragMinBound + hData.startMargin;
            const qreal maxX = hData.dragMaxBound - hData.endMargin;
            if (!(boundsBehavior & QQuickFlickable::DragOverBounds)) {
                if (fuzzyLessThanOrEqualTo(newX, maxX)) {
                    newX = maxX;
                    rejectX = hData.pressPos == maxX && hData.move.value() == maxX && dx < 0;
                }
                if (fuzzyLessThanOrEqualTo(minX, newX)) {
                    newX = minX;
                    rejectX |= hData.pressPos == minX && hData.move.value() == minX && dx > 0;
                }
            } else {
                qreal vel = velocity.x() / QML_FLICK_OVERSHOOTFRICTION;
                if (vel > 0. && vel > hData.velocity)
                    hData.velocity = qMin(velocity.x() / QML_FLICK_OVERSHOOTFRICTION, float(QML_FLICK_DEFAULTMAXVELOCITY));
                else if (vel < 0. && vel < hData.velocity)
                    hData.velocity = qMax(velocity.x() / QML_FLICK_OVERSHOOTFRICTION, -float(QML_FLICK_DEFAULTMAXVELOCITY));
                if (newX > minX) {
                    // Overshoot beyond the left.  But don't wait for momentum phase to end before returning to bounds.
                    if (momentum && hData.atBeginning) {
                        if (!hData.inRebound) {
                            hData.inRebound = true;
                            q->returnToBounds();
                        }
                        return;
                    }
                    if (velocitySensitiveOverBounds) {
                        qreal overshoot = (newX - minX) * hData.velocity / QML_FLICK_DEFAULTMAXVELOCITY / QML_FLICK_OVERSHOOTFRICTION;
                        overshoot = QML_FLICK_OVERSHOOT * devicePixelRatio() * EaseOvershoot(overshoot / QML_FLICK_OVERSHOOT / devicePixelRatio());
                        newX = minX + overshoot;
                    } else {
                        newX = minX + (newX - minX) / 2;
                    }
                } else if (newX < maxX && maxX - minX <= 0) {
                    // Overshoot beyond the right.  But don't wait for momentum phase to end before returning to bounds.
                    if (momentum && hData.atEnd) {
                        if (!hData.inRebound) {
                            hData.inRebound = true;
                            q->returnToBounds();
                        }
                        return;
                    }
                    if (velocitySensitiveOverBounds) {
                        qreal overshoot = (newX - maxX) * hData.velocity / QML_FLICK_DEFAULTMAXVELOCITY / QML_FLICK_OVERSHOOTFRICTION;
                        overshoot = QML_FLICK_OVERSHOOT * devicePixelRatio() * EaseOvershoot(overshoot / QML_FLICK_OVERSHOOT / devicePixelRatio());
                        newX = maxX - overshoot;
                    } else {
                        newX = maxX + (newX - maxX) / 2;
                    }
                }
            }

            if (!rejectX && stealMouse && dx != 0.0 && dx != hData.previousDragDelta) {
                clearTimeline();
                hData.move.setValue(newX);
                hMoved = true;
            }

            if (!rejectX && overThreshold)
                stealX = true;

            if ((newX >= minX && vData.pressPos == minX && vData.move.value() == minX && dx > 0)
                        || (newX <= maxX && vData.pressPos == maxX && vData.move.value() == maxX && dx < 0)) {
                keepX = false;
            }
        }
        hData.previousDragDelta = dx;
    }

    stealMouse = stealX || stealY;
    if (stealMouse) {
        if ((stealX && keepX) || (stealY && keepY))
            q->setKeepMouseGrab(true);
        clearDelayedPress();
    }

    if (rejectY) {
        vData.velocityBuffer.clear();
        vData.velocity = 0;
    }
    if (rejectX) {
        hData.velocityBuffer.clear();
        hData.velocity = 0;
    }

    if (momentum && !hData.flicking && !vData.flicking)
        flickingStarted(hData.velocity != 0, vData.velocity != 0);
    draggingStarting();

    if ((hMoved && !prevHMoved) || (vMoved && !prevVMoved))
        q->movementStarting();

    lastPosTime = currentTimestamp;
    if (q->yflick() && !rejectY)
        vData.addVelocitySample(velocity.y(), maxVelocity);
    if (q->xflick() && !rejectX)
        hData.addVelocitySample(velocity.x(), maxVelocity);
    lastPos = localPos;
}

void QQuickFlickablePrivate::handleMouseMoveEvent(QMouseEvent *event)
{
    Q_Q(QQuickFlickable);
    if (!interactive || lastPosTime == -1)
        return;

    qint64 currentTimestamp = computeCurrentTime(event);
    QVector2D deltas = QVector2D(event->localPos() - pressPos);
    bool overThreshold = false;
    QVector2D velocity = QGuiApplicationPrivate::mouseEventVelocity(event);
    // TODO guarantee that events always have velocity so that it never needs to be computed here
    if (!(QGuiApplicationPrivate::mouseEventCaps(event) & QTouchDevice::Velocity)) {
        qint64 lastTimestamp = (lastPos.isNull() ? lastPressTime : lastPosTime);
        if (currentTimestamp == lastTimestamp)
            return; // events are too close together: velocity would be infinite
        qreal elapsed = qreal(currentTimestamp - lastTimestamp) / 1000.;
        velocity = QVector2D(event->localPos() - (lastPos.isNull() ? pressPos : lastPos)) / elapsed;
    }

    if (q->yflick())
        overThreshold |= QQuickWindowPrivate::dragOverThreshold(deltas.y(), Qt::YAxis, event);
    if (q->xflick())
        overThreshold |= QQuickWindowPrivate::dragOverThreshold(deltas.x(), Qt::XAxis, event);

    drag(currentTimestamp, event->type(), event->localPos(), deltas, overThreshold, false, false, velocity);
}

void QQuickFlickablePrivate::handleMouseReleaseEvent(QMouseEvent *event)
{
    Q_Q(QQuickFlickable);
    stealMouse = false;
    q->setKeepMouseGrab(false);
    pressed = false;

    // if we drag then pause before release we should not cause a flick.
    qint64 elapsed = computeCurrentTime(event) - lastPosTime;

    vData.updateVelocity();
    hData.updateVelocity();

    draggingEnding();

    if (lastPosTime == -1)
        return;

    hData.vTime = vData.vTime = timeline.time();

    bool canBoost = false;

    qreal vVelocity = 0;
    if (elapsed < 100 && vData.velocity != 0.) {
        vVelocity = (QGuiApplicationPrivate::mouseEventCaps(event) & QTouchDevice::Velocity)
                ? QGuiApplicationPrivate::mouseEventVelocity(event).y() : vData.velocity;
    }
    if ((vData.atBeginning && vVelocity > 0.) || (vData.atEnd && vVelocity < 0.)) {
        vVelocity /= 2;
    } else if (vData.continuousFlickVelocity != 0.0
               && vData.viewSize/q->height() > QML_FLICK_MULTIFLICK_RATIO
               && ((vVelocity > 0) == (vData.continuousFlickVelocity > 0))
               && qAbs(vVelocity) > QML_FLICK_MULTIFLICK_THRESHOLD) {
        // accelerate flick for large view flicked quickly
        canBoost = true;
    }

    qreal hVelocity = 0;
    if (elapsed < 100 && hData.velocity != 0.) {
        hVelocity = (QGuiApplicationPrivate::mouseEventCaps(event) & QTouchDevice::Velocity)
                ? QGuiApplicationPrivate::mouseEventVelocity(event).x() : hData.velocity;
    }
    if ((hData.atBeginning && hVelocity > 0.) || (hData.atEnd && hVelocity < 0.)) {
        hVelocity /= 2;
    } else if (hData.continuousFlickVelocity != 0.0
               && hData.viewSize/q->width() > QML_FLICK_MULTIFLICK_RATIO
               && ((hVelocity > 0) == (hData.continuousFlickVelocity > 0))
               && qAbs(hVelocity) > QML_FLICK_MULTIFLICK_THRESHOLD) {
        // accelerate flick for large view flicked quickly
        canBoost = true;
    }

    flickBoost = canBoost ? qBound(1.0, flickBoost+0.25, QML_FLICK_MULTIFLICK_MAXBOOST) : 1.0;

    bool flickedVertically = false;
    vVelocity *= flickBoost;
    bool isVerticalFlickAllowed = q->yflick() && qAbs(vVelocity) > MinimumFlickVelocity && qAbs(event->localPos().y() - pressPos.y()) > FlickThreshold;
    if (isVerticalFlickAllowed) {
        velocityTimeline.reset(vData.smoothVelocity);
        vData.smoothVelocity.setValue(-vVelocity);
        flickedVertically = flickY(vVelocity);
    }

    bool flickedHorizontally = false;
    hVelocity *= flickBoost;
    bool isHorizontalFlickAllowed = q->xflick() && qAbs(hVelocity) > MinimumFlickVelocity && qAbs(event->localPos().x() - pressPos.x()) > FlickThreshold;
    if (isHorizontalFlickAllowed) {
        velocityTimeline.reset(hData.smoothVelocity);
        hData.smoothVelocity.setValue(-hVelocity);
        flickedHorizontally = flickX(hVelocity);
    }

    if (!isVerticalFlickAllowed)
        fixupY();

    if (!isHorizontalFlickAllowed)
        fixupX();

    flickingStarted(flickedHorizontally, flickedVertically);
    if (!isViewMoving())
        q->movementEnding();
}

void QQuickFlickable::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickFlickable);
    if (d->interactive) {
        if (!d->pressed)
            d->handleMousePressEvent(event);
        event->accept();
    } else {
        QQuickItem::mousePressEvent(event);
    }
}

void QQuickFlickable::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(QQuickFlickable);
    if (d->interactive) {
        d->handleMouseMoveEvent(event);
        event->accept();
    } else {
        QQuickItem::mouseMoveEvent(event);
    }
}

void QQuickFlickable::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickFlickable);
    if (d->interactive) {
        if (d->delayedPressEvent) {
            d->replayDelayedPress();

            // Now send the release
            if (window() && window()->mouseGrabberItem()) {
                QPointF localPos = window()->mouseGrabberItem()->mapFromScene(event->windowPos());
                QScopedPointer<QMouseEvent> mouseEvent(QQuickWindowPrivate::cloneMouseEvent(event, &localPos));
                QCoreApplication::sendEvent(window(), mouseEvent.data());
            }

            // And the event has been consumed
            d->stealMouse = false;
            d->pressed = false;
            return;
        }

        d->handleMouseReleaseEvent(event);
        event->accept();
    } else {
        QQuickItem::mouseReleaseEvent(event);
    }
}

#ifndef QT_NO_WHEELEVENT
void QQuickFlickable::wheelEvent(QWheelEvent *event)
{
    Q_D(QQuickFlickable);
    if (!d->interactive) {
        QQuickItem::wheelEvent(event);
        return;
    }
    event->setAccepted(false);
    qint64 currentTimestamp = d->computeCurrentTime(event);
    switch (event->phase()) {
    case Qt::ScrollBegin:
        d->scrollingPhase = true;
        d->accumulatedWheelPixelDelta = QVector2D();
        d->vData.velocity = 0;
        d->hData.velocity = 0;
        d->timer.start();
        d->maybeBeginDrag(currentTimestamp, event->posF());
        break;
    case Qt::NoScrollPhase: // default phase with an ordinary wheel mouse
    case Qt::ScrollUpdate:
        if (d->scrollingPhase)
            d->pressed = true;
#ifdef Q_OS_OSX
        d->movementEndingTimer.start(MovementEndingTimerInterval, this);
#endif
        break;
    case Qt::ScrollEnd:
        d->pressed = false;
        d->scrollingPhase = false;
        d->draggingEnding();
        event->accept();
        returnToBounds();
        d->lastPosTime = -1;
        return;
    }

    if (event->source() == Qt::MouseEventNotSynthesized || event->pixelDelta().isNull()) {
        // physical mouse wheel, so use angleDelta
        int xDelta = event->angleDelta().x();
        int yDelta = event->angleDelta().y();
        if (yflick() && yDelta != 0) {
            bool valid = false;
            if (yDelta > 0 && contentY() > -minYExtent()) {
                d->vData.velocity = qMax(yDelta*2 - d->vData.smoothVelocity.value(), qreal(d->maxVelocity/4));
                valid = true;
            } else if (yDelta < 0 && contentY() < -maxYExtent()) {
                d->vData.velocity = qMin(yDelta*2 - d->vData.smoothVelocity.value(), qreal(-d->maxVelocity/4));
                valid = true;
            }
            if (valid) {
                d->flickY(d->vData.velocity);
                d->flickingStarted(false, true);
                if (d->vData.flicking) {
                    d->vMoved = true;
                    movementStarting();
                }
                event->accept();
            }
        }
        if (xflick() && xDelta != 0) {
            bool valid = false;
            if (xDelta > 0 && contentX() > -minXExtent()) {
                d->hData.velocity = qMax(xDelta*2 - d->hData.smoothVelocity.value(), qreal(d->maxVelocity/4));
                valid = true;
            } else if (xDelta < 0 && contentX() < -maxXExtent()) {
                d->hData.velocity = qMin(xDelta*2 - d->hData.smoothVelocity.value(), qreal(-d->maxVelocity/4));
                valid = true;
            }
            if (valid) {
                d->flickX(d->hData.velocity);
                d->flickingStarted(true, false);
                if (d->hData.flicking) {
                    d->hMoved = true;
                    movementStarting();
                }
                event->accept();
            }
        }
    } else {
        // use pixelDelta (probably from a trackpad)
        int xDelta = event->pixelDelta().x();
        int yDelta = event->pixelDelta().y();

        qreal elapsed = qreal(currentTimestamp - d->lastPosTime) / 1000.;
        if (elapsed <= 0) {
            d->lastPosTime = currentTimestamp;
            return;
        }
        QVector2D velocity(xDelta / elapsed, yDelta / elapsed);
        d->lastPosTime = currentTimestamp;
        d->accumulatedWheelPixelDelta += QVector2D(event->pixelDelta());
        d->drag(currentTimestamp, event->type(), event->posF(), d->accumulatedWheelPixelDelta, true, !d->scrollingPhase, true, velocity);
        event->accept();
    }

    if (!event->isAccepted())
        QQuickItem::wheelEvent(event);
}
#endif

bool QQuickFlickablePrivate::isInnermostPressDelay(QQuickItem *i) const
{
    Q_Q(const QQuickFlickable);
    QQuickItem *item = i;
    while (item) {
        QQuickFlickable *flick = qobject_cast<QQuickFlickable*>(item);
        if (flick && flick->pressDelay() > 0 && flick->isInteractive()) {
            // Found the innermost flickable with press delay - is it me?
            return (flick == q);
        }
        item = item->parentItem();
    }
    return false;
}

void QQuickFlickablePrivate::captureDelayedPress(QQuickItem *item, QMouseEvent *event)
{
    Q_Q(QQuickFlickable);
    if (!q->window() || pressDelay <= 0)
        return;

    // Only the innermost flickable should handle the delayed press; this allows
    // flickables up the parent chain to all see the events in their filter functions
    if (!isInnermostPressDelay(item))
        return;

    delayedPressEvent = QQuickWindowPrivate::cloneMouseEvent(event);
    delayedPressEvent->setAccepted(false);
    delayedPressTimer.start(pressDelay, q);
}

void QQuickFlickablePrivate::clearDelayedPress()
{
    if (delayedPressEvent) {
        delayedPressTimer.stop();
        delete delayedPressEvent;
        delayedPressEvent = 0;
    }
}

void QQuickFlickablePrivate::replayDelayedPress()
{
    Q_Q(QQuickFlickable);
    if (delayedPressEvent) {
        // Losing the grab will clear the delayed press event; take control of it here
        QScopedPointer<QMouseEvent> mouseEvent(delayedPressEvent);
        delayedPressEvent = 0;
        delayedPressTimer.stop();

        // If we have the grab, release before delivering the event
        if (QQuickWindow *w = q->window()) {
            replayingPressEvent = true;
            if (w->mouseGrabberItem() == q)
                q->ungrabMouse();

            // Use the event handler that will take care of finding the proper item to propagate the event
            QCoreApplication::sendEvent(w, mouseEvent.data());
            replayingPressEvent = false;
        }
    }
}

//XXX pixelAligned ignores the global position of the Flickable, i.e. assumes Flickable itself is pixel aligned.
void QQuickFlickablePrivate::setViewportX(qreal x)
{
    contentItem->setX(pixelAligned ? -Round(-x) : x);
}

void QQuickFlickablePrivate::setViewportY(qreal y)
{
    contentItem->setY(pixelAligned ? -Round(-y) : y);
}

void QQuickFlickable::timerEvent(QTimerEvent *event)
{
    Q_D(QQuickFlickable);
    if (event->timerId() == d->delayedPressTimer.timerId()) {
        d->delayedPressTimer.stop();
        if (d->delayedPressEvent) {
            d->replayDelayedPress();
        }
    } else if (event->timerId() == d->movementEndingTimer.timerId()) {
        d->movementEndingTimer.stop();
        d->pressed = false;
        d->stealMouse = false;
        if (!d->velocityTimeline.isActive() && !d->timeline.isActive())
            movementEnding(true, true);
    }
}

qreal QQuickFlickable::minYExtent() const
{
    Q_D(const QQuickFlickable);
    return d->vData.startMargin;
}

qreal QQuickFlickable::minXExtent() const
{
    Q_D(const QQuickFlickable);
    return d->hData.startMargin;
}

/* returns -ve */
qreal QQuickFlickable::maxXExtent() const
{
    Q_D(const QQuickFlickable);
    return qMin<qreal>(minXExtent(), width() - vWidth() - d->hData.endMargin);
}
/* returns -ve */
qreal QQuickFlickable::maxYExtent() const
{
    Q_D(const QQuickFlickable);
    return qMin<qreal>(minYExtent(), height() - vHeight() - d->vData.endMargin);
}

void QQuickFlickable::componentComplete()
{
    Q_D(QQuickFlickable);
    QQuickItem::componentComplete();
    if (!d->hData.explicitValue && d->hData.startMargin != 0.)
        setContentX(-minXExtent());
    if (!d->vData.explicitValue && d->vData.startMargin != 0.)
        setContentY(-minYExtent());
}

void QQuickFlickable::viewportMoved(Qt::Orientations orient)
{
    Q_D(QQuickFlickable);
    if (orient & Qt::Vertical)
        d->viewportAxisMoved(d->vData, minYExtent(), maxYExtent(), height(), d->fixupY_callback);
    if (orient & Qt::Horizontal)
        d->viewportAxisMoved(d->hData, minXExtent(), maxXExtent(), width(), d->fixupX_callback);
    d->updateBeginningEnd();
}

void QQuickFlickablePrivate::viewportAxisMoved(AxisData &data, qreal minExtent, qreal maxExtent, qreal vSize,
                                           QQuickTimeLineCallback::Callback fixupCallback)
{
    if (!scrollingPhase && (pressed || calcVelocity)) {
        int elapsed = data.velocityTime.restart();
        if (elapsed > 0) {
            qreal velocity = (data.lastPos - data.move.value()) * 1000 / elapsed;
            if (qAbs(velocity) > 0) {
                velocityTimeline.reset(data.smoothVelocity);
                if (calcVelocity)
                    velocityTimeline.set(data.smoothVelocity, velocity);
                else
                    velocityTimeline.move(data.smoothVelocity, velocity, reportedVelocitySmoothing);
                velocityTimeline.move(data.smoothVelocity, 0, reportedVelocitySmoothing);
            }
        }
    } else {
        if (timeline.time() > data.vTime) {
            velocityTimeline.reset(data.smoothVelocity);
            qreal velocity = (data.lastPos - data.move.value()) * 1000 / (timeline.time() - data.vTime);
            data.smoothVelocity.setValue(velocity);
        }
    }

    if (!data.inOvershoot && !data.fixingUp && data.flicking
            && (data.move.value() > minExtent || data.move.value() < maxExtent)
            && qAbs(data.smoothVelocity.value()) > 10) {
        // Increase deceleration if we've passed a bound
        qreal overBound = data.move.value() > minExtent
                ? data.move.value() - minExtent
                : maxExtent - data.move.value();
        data.inOvershoot = true;
        qreal maxDistance = overShootDistance(vSize) - overBound;
        resetTimeline(data);
        if (maxDistance > 0)
            timeline.accel(data.move, -data.smoothVelocity.value(), deceleration*QML_FLICK_OVERSHOOTFRICTION, maxDistance);
        timeline.callback(QQuickTimeLineCallback(&data.move, fixupCallback, this));
    }

    data.lastPos = data.move.value();
    data.vTime = timeline.time();
}

void QQuickFlickable::geometryChanged(const QRectF &newGeometry,
                             const QRectF &oldGeometry)
{
    Q_D(QQuickFlickable);
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    bool changed = false;
    if (newGeometry.width() != oldGeometry.width()) {
        changed = true; // we must update visualArea.widthRatio
        if (d->hData.viewSize < 0)
            d->contentItem->setWidth(width());
        // Make sure that we're entirely in view.
        if (!d->pressed && !d->hData.moving && !d->vData.moving) {
            d->fixupMode = QQuickFlickablePrivate::Immediate;
            d->fixupX();
        }
    }
    if (newGeometry.height() != oldGeometry.height()) {
        changed = true; // we must update visualArea.heightRatio
        if (d->vData.viewSize < 0)
            d->contentItem->setHeight(height());
        // Make sure that we're entirely in view.
        if (!d->pressed && !d->hData.moving && !d->vData.moving) {
            d->fixupMode = QQuickFlickablePrivate::Immediate;
            d->fixupY();
        }
    }

    if (changed)
        d->updateBeginningEnd();
}

/*!
    \qmlmethod QtQuick::Flickable::flick(qreal xVelocity, qreal yVelocity)

    Flicks the content with \a xVelocity horizontally and \a yVelocity vertically in pixels/sec.

    Calling this method will update the corresponding moving and flicking properties and signals,
    just like a real flick.
*/

void QQuickFlickable::flick(qreal xVelocity, qreal yVelocity)
{
    Q_D(QQuickFlickable);
    d->hData.reset();
    d->vData.reset();
    d->hData.velocity = xVelocity;
    d->vData.velocity = yVelocity;

    bool flickedX = d->flickX(xVelocity);
    bool flickedY = d->flickY(yVelocity);

    if (flickedX)
        d->hMoved = true;
    if (flickedY)
        d->vMoved = true;
    movementStarting();
    d->flickingStarted(flickedX, flickedY);
}

void QQuickFlickablePrivate::flickingStarted(bool flickingH, bool flickingV)
{
    Q_Q(QQuickFlickable);
    if (!flickingH && !flickingV)
        return;

    bool wasFlicking = hData.flicking || vData.flicking;
    if (flickingH && !hData.flicking) {
        hData.flicking = true;
        emit q->flickingHorizontallyChanged();
    }
    if (flickingV && !vData.flicking) {
        vData.flicking = true;
        emit q->flickingVerticallyChanged();
    }
    if (!wasFlicking && (hData.flicking || vData.flicking)) {
        emit q->flickingChanged();
        emit q->flickStarted();
    }
}

/*!
    \qmlmethod QtQuick::Flickable::cancelFlick()

    Cancels the current flick animation.
*/

void QQuickFlickable::cancelFlick()
{
    Q_D(QQuickFlickable);
    d->resetTimeline(d->hData);
    d->resetTimeline(d->vData);
    movementEnding();
}

void QQuickFlickablePrivate::data_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    if (QQuickItem *i = qmlobject_cast<QQuickItem *>(o)) {
        i->setParentItem(static_cast<QQuickFlickablePrivate*>(prop->data)->contentItem);
    } else {
        o->setParent(prop->object); // XXX todo - do we want this?
    }
}

int QQuickFlickablePrivate::data_count(QQmlListProperty<QObject> *)
{
    // XXX todo
    return 0;
}

QObject *QQuickFlickablePrivate::data_at(QQmlListProperty<QObject> *, int)
{
    // XXX todo
    return 0;
}

void QQuickFlickablePrivate::data_clear(QQmlListProperty<QObject> *)
{
    // XXX todo
}

QQmlListProperty<QObject> QQuickFlickable::flickableData()
{
    Q_D(QQuickFlickable);
    return QQmlListProperty<QObject>(this, (void *)d, QQuickFlickablePrivate::data_append,
                                             QQuickFlickablePrivate::data_count,
                                             QQuickFlickablePrivate::data_at,
                                             QQuickFlickablePrivate::data_clear);
}

QQmlListProperty<QQuickItem> QQuickFlickable::flickableChildren()
{
    Q_D(QQuickFlickable);
    return QQuickItemPrivate::get(d->contentItem)->children();
}

/*!
    \qmlproperty enumeration QtQuick::Flickable::boundsBehavior
    This property holds whether the surface may be dragged
    beyond the Flickable's boundaries, or overshoot the
    Flickable's boundaries when flicked.

    This enables the feeling that the edges of the view are soft,
    rather than a hard physical boundary.

    The \c boundsBehavior can be one of:

    \list
    \li Flickable.StopAtBounds - the contents can not be dragged beyond the boundary
    of the flickable, and flicks will not overshoot.
    \li Flickable.DragOverBounds - the contents can be dragged beyond the boundary
    of the Flickable, but flicks will not overshoot.
    \li Flickable.OvershootBounds - the contents can overshoot the boundary when flicked,
    but the content cannot be dragged beyond the boundary of the flickable. (since \c{QtQuick 2.5})
    \li Flickable.DragAndOvershootBounds (default) - the contents can be dragged
    beyond the boundary of the Flickable, and can overshoot the
    boundary when flicked.
    \endlist
*/
QQuickFlickable::BoundsBehavior QQuickFlickable::boundsBehavior() const
{
    Q_D(const QQuickFlickable);
    return d->boundsBehavior;
}

void QQuickFlickable::setBoundsBehavior(BoundsBehavior b)
{
    Q_D(QQuickFlickable);
    if (b == d->boundsBehavior)
        return;
    d->boundsBehavior = b;
    emit boundsBehaviorChanged();
}

/*!
    \qmlproperty Transition QtQuick::Flickable::rebound

    This holds the transition to be applied to the content view when
    it snaps back to the bounds of the flickable. The transition is
    triggered when the view is flicked or dragged past the edge of the
    content area, or when returnToBounds() is called.

    \qml
    import QtQuick 2.0

    Flickable {
        width: 150; height: 150
        contentWidth: 300; contentHeight: 300

        rebound: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: 1000
                easing.type: Easing.OutBounce
            }
        }

        Rectangle {
            width: 300; height: 300
            gradient: Gradient {
                GradientStop { position: 0.0; color: "lightsteelblue" }
                GradientStop { position: 1.0; color: "blue" }
            }
        }
    }
    \endqml

    When the above view is flicked beyond its bounds, it will return to its
    bounds using the transition specified:

    \image flickable-rebound.gif

    If this property is not set, a default animation is applied.
  */
QQuickTransition *QQuickFlickable::rebound() const
{
    Q_D(const QQuickFlickable);
    return d->rebound;
}

void QQuickFlickable::setRebound(QQuickTransition *transition)
{
    Q_D(QQuickFlickable);
    if (transition) {
        if (!d->hData.transitionToBounds)
            d->hData.transitionToBounds = new QQuickFlickableReboundTransition(this, QLatin1String("x"));
        if (!d->vData.transitionToBounds)
            d->vData.transitionToBounds = new QQuickFlickableReboundTransition(this, QLatin1String("y"));
    }
    if (d->rebound != transition) {
        d->rebound = transition;
        emit reboundChanged();
    }
}

/*!
    \qmlproperty real QtQuick::Flickable::contentWidth
    \qmlproperty real QtQuick::Flickable::contentHeight

    The dimensions of the content (the surface controlled by Flickable).
    This should typically be set to the combined size of the items placed in the
    Flickable.

    The following snippet shows how these properties are used to display
    an image that is larger than the Flickable item itself:

    \snippet qml/flickable.qml document

    In some cases, the content dimensions can be automatically set
    based on the \l {Item::childrenRect.width}{childrenRect.width}
    and \l {Item::childrenRect.height}{childrenRect.height} properties
    of the \l contentItem. For example, the previous snippet could be rewritten with:

    \code
    contentWidth: contentItem.childrenRect.width; contentHeight: contentItem.childrenRect.height
    \endcode

    Though this assumes that the origin of the childrenRect is 0,0.
*/
qreal QQuickFlickable::contentWidth() const
{
    Q_D(const QQuickFlickable);
    return d->hData.viewSize;
}

void QQuickFlickable::setContentWidth(qreal w)
{
    Q_D(QQuickFlickable);
    if (d->hData.viewSize == w)
        return;
    d->hData.viewSize = w;
    if (w < 0)
        d->contentItem->setWidth(width());
    else
        d->contentItem->setWidth(w);
    d->hData.markExtentsDirty();
    // Make sure that we're entirely in view.
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupX();
    } else if (!d->pressed && d->hData.fixingUp) {
        d->fixupMode = QQuickFlickablePrivate::ExtentChanged;
        d->fixupX();
    }
    emit contentWidthChanged();
    d->updateBeginningEnd();
}

qreal QQuickFlickable::contentHeight() const
{
    Q_D(const QQuickFlickable);
    return d->vData.viewSize;
}

void QQuickFlickable::setContentHeight(qreal h)
{
    Q_D(QQuickFlickable);
    if (d->vData.viewSize == h)
        return;
    d->vData.viewSize = h;
    if (h < 0)
        d->contentItem->setHeight(height());
    else
        d->contentItem->setHeight(h);
    d->vData.markExtentsDirty();
    // Make sure that we're entirely in view.
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupY();
    } else if (!d->pressed && d->vData.fixingUp) {
        d->fixupMode = QQuickFlickablePrivate::ExtentChanged;
        d->fixupY();
    }
    emit contentHeightChanged();
    d->updateBeginningEnd();
}

/*!
    \qmlproperty real QtQuick::Flickable::topMargin
    \qmlproperty real QtQuick::Flickable::leftMargin
    \qmlproperty real QtQuick::Flickable::bottomMargin
    \qmlproperty real QtQuick::Flickable::rightMargin

    These properties hold the margins around the content.  This space is reserved
    in addition to the contentWidth and contentHeight.
*/


qreal QQuickFlickable::topMargin() const
{
    Q_D(const QQuickFlickable);
    return d->vData.startMargin;
}

void QQuickFlickable::setTopMargin(qreal m)
{
    Q_D(QQuickFlickable);
    if (d->vData.startMargin == m)
        return;
    d->vData.startMargin = m;
    d->vData.markExtentsDirty();
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupY();
    }
    emit topMarginChanged();
    d->updateBeginningEnd();
}

qreal QQuickFlickable::bottomMargin() const
{
    Q_D(const QQuickFlickable);
    return d->vData.endMargin;
}

void QQuickFlickable::setBottomMargin(qreal m)
{
    Q_D(QQuickFlickable);
    if (d->vData.endMargin == m)
        return;
    d->vData.endMargin = m;
    d->vData.markExtentsDirty();
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupY();
    }
    emit bottomMarginChanged();
    d->updateBeginningEnd();
}

qreal QQuickFlickable::leftMargin() const
{
    Q_D(const QQuickFlickable);
    return d->hData.startMargin;
}

void QQuickFlickable::setLeftMargin(qreal m)
{
    Q_D(QQuickFlickable);
    if (d->hData.startMargin == m)
        return;
    d->hData.startMargin = m;
    d->hData.markExtentsDirty();
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupX();
    }
    emit leftMarginChanged();
    d->updateBeginningEnd();
}

qreal QQuickFlickable::rightMargin() const
{
    Q_D(const QQuickFlickable);
    return d->hData.endMargin;
}

void QQuickFlickable::setRightMargin(qreal m)
{
    Q_D(QQuickFlickable);
    if (d->hData.endMargin == m)
        return;
    d->hData.endMargin = m;
    d->hData.markExtentsDirty();
    if (!d->pressed && !d->hData.moving && !d->vData.moving) {
        d->fixupMode = QQuickFlickablePrivate::Immediate;
        d->fixupX();
    }
    emit rightMarginChanged();
    d->updateBeginningEnd();
}

/*!
    \qmlproperty real QtQuick::Flickable::originX
    \qmlproperty real QtQuick::Flickable::originY

    These properties hold the origin of the content. This value always refers
    to the top-left position of the content regardless of layout direction.

    This is usually (0,0), however ListView and GridView may have an arbitrary
    origin due to delegate size variation, or item insertion/removal outside
    the visible region.
*/

qreal QQuickFlickable::originY() const
{
    Q_D(const QQuickFlickable);
    return -minYExtent() + d->vData.startMargin;
}

qreal QQuickFlickable::originX() const
{
    Q_D(const QQuickFlickable);
    return -minXExtent() + d->hData.startMargin;
}


/*!
    \qmlmethod QtQuick::Flickable::resizeContent(real width, real height, QPointF center)

    Resizes the content to \a width x \a height about \a center.

    This does not scale the contents of the Flickable - it only resizes the \l contentWidth
    and \l contentHeight.

    Resizing the content may result in the content being positioned outside
    the bounds of the Flickable.  Calling \l returnToBounds() will
    move the content back within legal bounds.
*/
void QQuickFlickable::resizeContent(qreal w, qreal h, QPointF center)
{
    Q_D(QQuickFlickable);
    if (w != d->hData.viewSize) {
        qreal oldSize = d->hData.viewSize;
        d->hData.viewSize = w;
        d->contentItem->setWidth(w);
        emit contentWidthChanged();
        if (center.x() != 0) {
            qreal pos = center.x() * w / oldSize;
            setContentX(contentX() + pos - center.x());
        }
    }
    if (h != d->vData.viewSize) {
        qreal oldSize = d->vData.viewSize;
        d->vData.viewSize = h;
        d->contentItem->setHeight(h);
        emit contentHeightChanged();
        if (center.y() != 0) {
            qreal pos = center.y() * h / oldSize;
            setContentY(contentY() + pos - center.y());
        }
    }
    d->updateBeginningEnd();
}

/*!
    \qmlmethod QtQuick::Flickable::returnToBounds()

    Ensures the content is within legal bounds.

    This may be called to ensure that the content is within legal bounds
    after manually positioning the content.
*/
void QQuickFlickable::returnToBounds()
{
    Q_D(QQuickFlickable);
    d->fixupX();
    d->fixupY();
}

qreal QQuickFlickable::vWidth() const
{
    Q_D(const QQuickFlickable);
    if (d->hData.viewSize < 0)
        return width();
    else
        return d->hData.viewSize;
}

qreal QQuickFlickable::vHeight() const
{
    Q_D(const QQuickFlickable);
    if (d->vData.viewSize < 0)
        return height();
    else
        return d->vData.viewSize;
}

bool QQuickFlickable::xflick() const
{
    Q_D(const QQuickFlickable);
    if ((d->flickableDirection & QQuickFlickable::AutoFlickIfNeeded) && (vWidth() > width()))
        return true;
    if (d->flickableDirection == QQuickFlickable::AutoFlickDirection)
        return std::floor(qAbs(vWidth() - width()));
    return d->flickableDirection & QQuickFlickable::HorizontalFlick;
}

bool QQuickFlickable::yflick() const
{
    Q_D(const QQuickFlickable);
    if ((d->flickableDirection & QQuickFlickable::AutoFlickIfNeeded) && (vHeight() > height()))
        return true;
    if (d->flickableDirection == QQuickFlickable::AutoFlickDirection)
        return std::floor(qAbs(vHeight() - height()));
    return d->flickableDirection & QQuickFlickable::VerticalFlick;
}

void QQuickFlickable::mouseUngrabEvent()
{
    Q_D(QQuickFlickable);
    // if our mouse grab has been removed (probably by another Flickable),
    // fix our state
    if (!d->replayingPressEvent)
        d->cancelInteraction();
}

void QQuickFlickablePrivate::cancelInteraction()
{
    Q_Q(QQuickFlickable);
    if (pressed) {
        clearDelayedPress();
        pressed = false;
        draggingEnding();
        stealMouse = false;
        q->setKeepMouseGrab(false);
        fixupX();
        fixupY();
        if (!isViewMoving())
            q->movementEnding();
    }
}

/*!
    QQuickFlickable::filterMouseEvent checks filtered mouse events and potentially steals them.

    This is how flickable takes over events from other items (\a receiver) that are on top of it.
    It filters their events and may take over (grab) the \a event.
    Return true if the mouse event will be stolen.
    \internal
*/
bool QQuickFlickable::filterMouseEvent(QQuickItem *receiver, QMouseEvent *event)
{
    Q_D(QQuickFlickable);
    QPointF localPos = mapFromScene(event->windowPos());

    Q_ASSERT_X(receiver != this, "", "Flickable received a filter event for itself");
    if (receiver == this && d->stealMouse) {
        // we are already the grabber and we do want the mouse event to ourselves.
        return true;
    }

    bool receiverDisabled = receiver && !receiver->isEnabled();
    bool stealThisEvent = d->stealMouse;
    if ((stealThisEvent || contains(localPos)) && (!receiver || !receiver->keepMouseGrab() || receiverDisabled)) {
        QScopedPointer<QMouseEvent> mouseEvent(QQuickWindowPrivate::cloneMouseEvent(event, &localPos));
        mouseEvent->setAccepted(false);

        switch (mouseEvent->type()) {
        case QEvent::MouseMove:
            d->handleMouseMoveEvent(mouseEvent.data());
            break;
        case QEvent::MouseButtonPress:
            d->handleMousePressEvent(mouseEvent.data());
            d->captureDelayedPress(receiver, event);
            stealThisEvent = d->stealMouse;   // Update stealThisEvent in case changed by function call above
            break;
        case QEvent::MouseButtonRelease:
            d->handleMouseReleaseEvent(mouseEvent.data());
            stealThisEvent = d->stealMouse;
            break;
        default:
            break;
        }
        if ((receiver && stealThisEvent && !receiver->keepMouseGrab() && receiver != this) || receiverDisabled) {
            d->clearDelayedPress();
            grabMouse();
        } else if (d->delayedPressEvent) {
            grabMouse();
        }

        const bool filtered = stealThisEvent || d->delayedPressEvent || receiverDisabled;
        if (filtered) {
            event->setAccepted(true);
        }
        return filtered;
    } else if (d->lastPosTime != -1) {
        d->lastPosTime = -1;
        returnToBounds();
    }
    if (event->type() == QEvent::MouseButtonRelease || (receiver && receiver->keepMouseGrab() && !receiverDisabled)) {
        // mouse released, or another item has claimed the grab
        d->lastPosTime = -1;
        d->clearDelayedPress();
        d->stealMouse = false;
        d->pressed = false;
    }
    return false;
}


bool QQuickFlickable::childMouseEventFilter(QQuickItem *i, QEvent *e)
{
    Q_D(QQuickFlickable);
    if (!isVisible() || !isEnabled() || !isInteractive())
        return QQuickItem::childMouseEventFilter(i, e);
    switch (e->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        return filterMouseEvent(i, static_cast<QMouseEvent *>(e));
    case QEvent::UngrabMouse:
        if (d->window && d->window->mouseGrabberItem() && d->window->mouseGrabberItem() != this) {
            // The grab has been taken away from a child and given to some other item.
            mouseUngrabEvent();
        }
        break;
    default:
        break;
    }

    return QQuickItem::childMouseEventFilter(i, e);
}

/*!
    \qmlproperty real QtQuick::Flickable::maximumFlickVelocity
    This property holds the maximum velocity that the user can flick the view in pixels/second.

    The default value is platform dependent.
*/
qreal QQuickFlickable::maximumFlickVelocity() const
{
    Q_D(const QQuickFlickable);
    return d->maxVelocity;
}

void QQuickFlickable::setMaximumFlickVelocity(qreal v)
{
    Q_D(QQuickFlickable);
    if (v == d->maxVelocity)
        return;
    d->maxVelocity = v;
    emit maximumFlickVelocityChanged();
}

/*!
    \qmlproperty real QtQuick::Flickable::flickDeceleration
    This property holds the rate at which a flick will decelerate.

    The default value is platform dependent.
*/
qreal QQuickFlickable::flickDeceleration() const
{
    Q_D(const QQuickFlickable);
    return d->deceleration;
}

void QQuickFlickable::setFlickDeceleration(qreal deceleration)
{
    Q_D(QQuickFlickable);
    if (deceleration == d->deceleration)
        return;
    d->deceleration = deceleration;
    emit flickDecelerationChanged();
}

bool QQuickFlickable::isFlicking() const
{
    Q_D(const QQuickFlickable);
    return d->hData.flicking ||  d->vData.flicking;
}

/*!
    \qmlproperty bool QtQuick::Flickable::flicking
    \qmlproperty bool QtQuick::Flickable::flickingHorizontally
    \qmlproperty bool QtQuick::Flickable::flickingVertically

    These properties describe whether the view is currently moving horizontally,
    vertically or in either direction, due to the user flicking the view.
*/
bool QQuickFlickable::isFlickingHorizontally() const
{
    Q_D(const QQuickFlickable);
    return d->hData.flicking;
}

bool QQuickFlickable::isFlickingVertically() const
{
    Q_D(const QQuickFlickable);
    return d->vData.flicking;
}

/*!
    \qmlproperty bool QtQuick::Flickable::dragging
    \qmlproperty bool QtQuick::Flickable::draggingHorizontally
    \qmlproperty bool QtQuick::Flickable::draggingVertically

    These properties describe whether the view is currently moving horizontally,
    vertically or in either direction, due to the user dragging the view.
*/
bool QQuickFlickable::isDragging() const
{
    Q_D(const QQuickFlickable);
    return d->hData.dragging ||  d->vData.dragging;
}

bool QQuickFlickable::isDraggingHorizontally() const
{
    Q_D(const QQuickFlickable);
    return d->hData.dragging;
}

bool QQuickFlickable::isDraggingVertically() const
{
    Q_D(const QQuickFlickable);
    return d->vData.dragging;
}

void QQuickFlickablePrivate::draggingStarting()
{
    Q_Q(QQuickFlickable);
    bool wasDragging = hData.dragging || vData.dragging;
    if (hMoved && !hData.dragging) {
        hData.dragging = true;
        emit q->draggingHorizontallyChanged();
    }
    if (vMoved && !vData.dragging) {
        vData.dragging = true;
        emit q->draggingVerticallyChanged();
    }
    if (!wasDragging && (hData.dragging || vData.dragging)) {
        emit q->draggingChanged();
        emit q->dragStarted();
    }
}

void QQuickFlickablePrivate::draggingEnding()
{
    Q_Q(QQuickFlickable);
    bool wasDragging = hData.dragging || vData.dragging;
    if (hData.dragging) {
        hData.dragging = false;
        emit q->draggingHorizontallyChanged();
    }
    if (vData.dragging) {
        vData.dragging = false;
        emit q->draggingVerticallyChanged();
    }
    if (wasDragging && !hData.dragging && !vData.dragging) {
        emit q->draggingChanged();
        emit q->dragEnded();
    }
    hData.inRebound = false;
    vData.inRebound = false;
}

bool QQuickFlickablePrivate::isViewMoving() const
{
    if (timeline.isActive()
            || (hData.transitionToBounds && hData.transitionToBounds->isActive())
            || (vData.transitionToBounds && vData.transitionToBounds->isActive()) ) {
        return true;
    }
    return false;
}

/*!
    \qmlproperty int QtQuick::Flickable::pressDelay

    This property holds the time to delay (ms) delivering a press to
    children of the Flickable.  This can be useful where reacting
    to a press before a flicking action has undesirable effects.

    If the flickable is dragged/flicked before the delay times out
    the press event will not be delivered.  If the button is released
    within the timeout, both the press and release will be delivered.

    Note that for nested Flickables with pressDelay set, the pressDelay of
    outer Flickables is overridden by the innermost Flickable. If the drag
    exceeds the platform drag threshold, the press event will be delivered
    regardless of this property.

    \sa QStyleHints
*/
int QQuickFlickable::pressDelay() const
{
    Q_D(const QQuickFlickable);
    return d->pressDelay;
}

void QQuickFlickable::setPressDelay(int delay)
{
    Q_D(QQuickFlickable);
    if (d->pressDelay == delay)
        return;
    d->pressDelay = delay;
    emit pressDelayChanged();
}

/*!
    \qmlproperty bool QtQuick::Flickable::moving
    \qmlproperty bool QtQuick::Flickable::movingHorizontally
    \qmlproperty bool QtQuick::Flickable::movingVertically

    These properties describe whether the view is currently moving horizontally,
    vertically or in either direction, due to the user either dragging or
    flicking the view.
*/

bool QQuickFlickable::isMoving() const
{
    Q_D(const QQuickFlickable);
    return d->hData.moving || d->vData.moving;
}

bool QQuickFlickable::isMovingHorizontally() const
{
    Q_D(const QQuickFlickable);
    return d->hData.moving;
}

bool QQuickFlickable::isMovingVertically() const
{
    Q_D(const QQuickFlickable);
    return d->vData.moving;
}

void QQuickFlickable::velocityTimelineCompleted()
{
    Q_D(QQuickFlickable);
    if ( (d->hData.transitionToBounds && d->hData.transitionToBounds->isActive())
         || (d->vData.transitionToBounds && d->vData.transitionToBounds->isActive()) ) {
        return;
    }
    // With subclasses such as GridView, velocityTimeline.completed is emitted repeatedly:
    // for example setting currentIndex results in a visual "flick" which the user
    // didn't initiate directly. We don't want to end movement repeatedly, and in
    // that case movementEnding will happen after the sequence of movements ends.
    if (d->vData.flicking)
        movementEnding();
    d->updateBeginningEnd();
}

void QQuickFlickable::timelineCompleted()
{
    Q_D(QQuickFlickable);
    if ( (d->hData.transitionToBounds && d->hData.transitionToBounds->isActive())
         || (d->vData.transitionToBounds && d->vData.transitionToBounds->isActive()) ) {
        return;
    }
    movementEnding();
    d->updateBeginningEnd();
}

void QQuickFlickable::movementStarting()
{
    Q_D(QQuickFlickable);
    bool wasMoving = d->hData.moving || d->vData.moving;
    if (d->hMoved && !d->hData.moving) {
        d->hData.moving = true;
        emit movingHorizontallyChanged();
    }
    if (d->vMoved && !d->vData.moving) {
        d->vData.moving = true;
        emit movingVerticallyChanged();
    }

    if (!wasMoving && (d->hData.moving || d->vData.moving)) {
        emit movingChanged();
        emit movementStarted();
    }
}

void QQuickFlickable::movementEnding()
{
    movementEnding(true, true);
}

void QQuickFlickable::movementEnding(bool hMovementEnding, bool vMovementEnding)
{
    Q_D(QQuickFlickable);

    // emit flicking signals
    bool wasFlicking = d->hData.flicking || d->vData.flicking;
    if (hMovementEnding && d->hData.flicking) {
        d->hData.flicking = false;
        emit flickingHorizontallyChanged();
    }
    if (vMovementEnding && d->vData.flicking) {
        d->vData.flicking = false;
        emit flickingVerticallyChanged();
    }
    if (wasFlicking && (!d->hData.flicking || !d->vData.flicking)) {
        emit flickingChanged();
        emit flickEnded();
    }

    // emit moving signals
    bool wasMoving = isMoving();
    if (hMovementEnding && d->hData.moving
            && (!d->pressed && !d->stealMouse)) {
        d->hData.moving = false;
        d->hMoved = false;
        emit movingHorizontallyChanged();
    }
    if (vMovementEnding && d->vData.moving
            && (!d->pressed && !d->stealMouse)) {
        d->vData.moving = false;
        d->vMoved = false;
        emit movingVerticallyChanged();
    }
    if (wasMoving && !isMoving()) {
        emit movingChanged();
        emit movementEnded();
    }

    if (hMovementEnding) {
        d->hData.fixingUp = false;
        d->hData.smoothVelocity.setValue(0);
        d->hData.previousDragDelta = 0.0;
    }
    if (vMovementEnding) {
        d->vData.fixingUp = false;
        d->vData.smoothVelocity.setValue(0);
        d->vData.previousDragDelta = 0.0;
    }
}

void QQuickFlickablePrivate::updateVelocity()
{
    Q_Q(QQuickFlickable);
    emit q->horizontalVelocityChanged();
    emit q->verticalVelocityChanged();
}

QT_END_NAMESPACE
