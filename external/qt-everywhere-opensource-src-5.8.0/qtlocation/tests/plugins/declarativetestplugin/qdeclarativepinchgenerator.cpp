/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativepinchgenerator_p.h"

#include <QtTest/QtTest>
#include <QtGui/QGuiApplication>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QtGui/QStyleHints>

QT_BEGIN_NAMESPACE

QDeclarativePinchGenerator::QDeclarativePinchGenerator():
    target_(0),
    state_(Invalid),
    window_(0),
    activeSwipe_(0),
    replayTimer_(-1),
    replayBookmark_(-1),
    masterSwipe_(-1),
    replaySpeedFactor_(1.0),
    enabled_(true)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MidButton | Qt::RightButton);
    swipeTimer_.invalidate();
    device_ = new QTouchDevice;
    device_->setType(QTouchDevice::TouchScreen);
    QWindowSystemInterface::registerTouchDevice(device_);
}

QDeclarativePinchGenerator::~QDeclarativePinchGenerator()
{
    clear();
}

void QDeclarativePinchGenerator::componentComplete()
{
    QQuickItem::componentComplete();
}

void QDeclarativePinchGenerator::mousePressEvent(QMouseEvent *event)
{
    if (state_ != Idle || !enabled_) {
        event->ignore();
        return;
    }
    Q_ASSERT(!activeSwipe_);
    Q_ASSERT(!swipeTimer_.isValid());
    // Start recording a pinch gesture.
    activeSwipe_ = new Swipe;
    activeSwipe_->touchPoints << event->pos();
    activeSwipe_->durations << 0;
    swipeTimer_.start();
    setState(Recording);
}

void QDeclarativePinchGenerator::mouseMoveEvent(QMouseEvent *event)
{
    if (state_ != Recording || !enabled_) {
        event->ignore();
        return;
    }
    Q_ASSERT(activeSwipe_);
    Q_ASSERT(swipeTimer_.isValid());

    activeSwipe_->touchPoints << event->pos();
    activeSwipe_->durations << swipeTimer_.elapsed();
    swipeTimer_.restart();
}

void QDeclarativePinchGenerator::mouseReleaseEvent(QMouseEvent *event)
{
    if (state_ != Recording || !enabled_) {
        event->ignore();
        return;
    }
    Q_ASSERT(activeSwipe_);
    Q_ASSERT(swipeTimer_.isValid());
    activeSwipe_->touchPoints << event->pos();
    activeSwipe_->durations << swipeTimer_.elapsed();

    if (swipes_.count() == SWIPES_REQUIRED)
        delete swipes_.takeFirst();
    swipes_ << activeSwipe_;
    activeSwipe_ = 0;
    swipeTimer_.invalidate();
    if (window_ && target_) setState(Idle); else setState(Invalid);
}

void QDeclarativePinchGenerator::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!enabled_) {
        event->ignore();
        return;
    }
    stop();
    clear();
    if (window_ && target_) setState(Idle); else setState(Invalid);
}

void QDeclarativePinchGenerator::keyPressEvent(QKeyEvent *e)
{
    if (!enabled_) {
        e->ignore();
    }

    if (e->key() == Qt::Key_C) {
        clear();
    } else if (e->key() == Qt::Key_R) {
        replay();
    } else if (e->key() == Qt::Key_S) {
        stop();
    } else if (e->key() == Qt::Key_Plus) {
        setReplaySpeedFactor(replaySpeedFactor() + 0.1);
    } else if (e->key() == Qt::Key_Minus) {
        setReplaySpeedFactor(replaySpeedFactor() - 0.1);
    } else {
        qDebug() << metaObject()->className() << "Unsupported key event.";
    }
}

bool QDeclarativePinchGenerator::enabled() const
{
    return enabled_;
}


void QDeclarativePinchGenerator::setEnabled(bool enabled)
{
    if (enabled == enabled_)
        return;
    enabled_ = enabled;
    if (!enabled_) {
        stop();
        clear();
    }
    emit enabledChanged();
}


qreal QDeclarativePinchGenerator::replaySpeedFactor() const
{
    return replaySpeedFactor_;
}

void QDeclarativePinchGenerator::setReplaySpeedFactor(qreal factor)
{
    if (factor == replaySpeedFactor_ || factor < 0.001)
        return;
    replaySpeedFactor_ = factor;
    emit replaySpeedFactorChanged();
}


QString QDeclarativePinchGenerator::state() const
{
    switch (state_) {
    case Invalid:
        return "Invalid";
    case Idle:
        return "Idle";
        break;
    case Recording:
        return "Recording";
        break;
    case Replaying:
        return "Replaying";
        break;
    default:
        Q_ASSERT(false);
    }
    return "How emberassing";
}

void QDeclarativePinchGenerator::setState(GeneratorState state)
{
    if (state == state_)
        return;
    state_ = state;
    emit stateChanged();
}

void QDeclarativePinchGenerator::itemChange(ItemChange change, const ItemChangeData & data)
{
    if (change == ItemSceneChange) {
        window_ = data.window;
        if (target_)
            setState(Idle);
    }
}

void QDeclarativePinchGenerator::timerEvent(QTimerEvent *event)
{
    Q_ASSERT(replayTimer_ == event->timerId());
    Q_ASSERT(state_ == Replaying);

    int slaveSwipe = masterSwipe_ ^ 1;

    int masterCount = swipes_.at(masterSwipe_)->touchPoints.count();
    int slaveCount = swipes_.at(slaveSwipe)->touchPoints.count();

    if (replayBookmark_ == 0) {
        QTest::touchEvent(window_, device_)
                .press(0, swipes_.at(masterSwipe_)->touchPoints.at(replayBookmark_))
                .press(1, swipes_.at(slaveSwipe)->touchPoints.at(replayBookmark_));
    } else if (replayBookmark_ == (slaveCount - 1)) {
        if (masterCount != slaveCount) {
            QTest::touchEvent(window_, device_)
                    .move(0, swipes_.at(masterSwipe_)->touchPoints.at(replayBookmark_))
                    .release(1, swipes_.at(slaveSwipe)->touchPoints.at(replayBookmark_));
        } else {
            QTest::touchEvent(window_, device_)
                    .release(0, swipes_.at(masterSwipe_)->touchPoints.at(replayBookmark_))
                    .release(1, swipes_.at(slaveSwipe)->touchPoints.at(replayBookmark_));
        }
    } else if (replayBookmark_ == (masterCount - 1)) {
            QTest::touchEvent(window_, device_)
                    .release(0, swipes_.at(masterSwipe_)->touchPoints.at(replayBookmark_));
    }
    else {
        QTest::touchEvent(window_, device_)
                .move(0, swipes_.at(masterSwipe_)->touchPoints.at(replayBookmark_))
                .move(1, swipes_.at(slaveSwipe)->touchPoints.at(replayBookmark_));
    }

    replayBookmark_++;
    if (replayBookmark_ >= swipes_.at(masterSwipe_)->touchPoints.count())
        stop();
    else {
        killTimer(replayTimer_);
        replayTimer_ = startTimer((swipes_.at(masterSwipe_)->durations.at(replayBookmark_) + 5) / replaySpeedFactor_ );
    }
}

QQuickItem* QDeclarativePinchGenerator::target() const
{
    return target_;
}

void QDeclarativePinchGenerator::setTarget(QQuickItem* target)
{
    if (target == target_)
        return;
    target_ = target;
    stop();
    clear();
    if (window_)
        setState(Idle);
    else
        setState(Invalid);
    emit targetChanged();
}

void QDeclarativePinchGenerator::pinch(QPoint point1From,
                                       QPoint point1To,
                                       QPoint point2From,
                                       QPoint point2To,
                                       int interval1,
                                       int interval2,
                                       int samples1,
                                       int samples2)
{
    Q_ASSERT(interval1 > 10);
    Q_ASSERT(interval2 > 10);
    Q_ASSERT(samples1 >= 2); // we need press and release events at minimum
    Q_ASSERT(samples2 >= 2);

    clear();

    Swipe* swipe1 = new Swipe;
    Swipe* swipe2 = new Swipe;
    for (int i = 0; i < samples1; ++i) {
        swipe1->touchPoints << point1From + (point1To - point1From) / samples1 * i;
        swipe1->durations << interval1;
    }
    for (int i = 0; i < samples2; ++i) {
        swipe2->touchPoints << point2From + (point2To - point2From) / samples2 * i;
        swipe2->durations << interval2;
    }
    swipes_ << swipe1 << swipe2;
    Q_ASSERT(swipes_.at(0));
    Q_ASSERT(swipes_.at(1));

    masterSwipe_ = (samples1 >= samples2) ? 0 : 1;

    replayTimer_ = startTimer(swipes_.at(masterSwipe_)->durations.at(0) / replaySpeedFactor_);
    replayBookmark_ = 0;
    setState(Replaying);
}

void QDeclarativePinchGenerator::pinchPress(QPoint point1From, QPoint point2From)
{
    QTest::touchEvent(window_, device_).press(0, point1From).press(1, point2From);
}

void QDeclarativePinchGenerator::pinchMoveTo(QPoint point1To, QPoint point2To)
{
    QTest::touchEvent(window_, device_).move(0, point1To).move(1, point2To);
}

void QDeclarativePinchGenerator::pinchRelease(QPoint point1To, QPoint point2To)
{
    QTest::touchEvent(window_, device_).release(0, point1To).release(1, point2To);
}

void QDeclarativePinchGenerator::replay()
{
    if (state_ != Idle) {
        qDebug() << "Wrong state, will not replay pinch, state: " << state_;
        return;
    }
    if (swipes_.count() < SWIPES_REQUIRED) {
        qDebug() << "Too few swipes, cannot replay, amount: " << swipes_.count();
        return;
    }
    if ((swipes_.at(0)->touchPoints.count() < 2) || (swipes_.at(1)->touchPoints.count() < 2)) {
        qDebug() << "Too few touchpoints, won't replay, amount: " <<
                    swipes_.at(0)->touchPoints.count() << (swipes_.at(1)->touchPoints.count() < 2);
        return;
    }

    masterSwipe_ = (swipes_.at(0)->touchPoints.count() >= swipes_.at(1)->touchPoints.count()) ? 0 : 1;

    replayTimer_ = startTimer(swipes_.at(masterSwipe_)->touchPoints.count() / replaySpeedFactor_);
    replayBookmark_ = 0;
    setState(Replaying);
}

void QDeclarativePinchGenerator::clear()
{
    stop();
    delete activeSwipe_;
    activeSwipe_ = 0;
    if (!swipes_.isEmpty()) {
        qDeleteAll(swipes_);
        swipes_.clear();
    }
}

void QDeclarativePinchGenerator::stop()
{
    if (state_ != Replaying)
        return;
    // stop replay
    Q_ASSERT(replayTimer_ != -1);
    killTimer(replayTimer_);
    replayTimer_ = -1;
    setState(Idle);
}

int QDeclarativePinchGenerator::startDragDistance()
{
    return qApp->styleHints()->startDragDistance();
}

QT_END_NAMESPACE
