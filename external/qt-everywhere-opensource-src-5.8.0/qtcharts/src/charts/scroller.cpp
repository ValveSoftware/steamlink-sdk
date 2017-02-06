/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/scroller_p.h>
#include <QtCharts/QLegend>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

Scroller::Scroller()
    : m_ticker(this),
      m_timeTresholdMin(50),
      m_timeTresholdMax(300),
      m_state(Idle),
      m_treshold(10)
{

}

Scroller::~Scroller()
{
}

void Scroller::move(const QPointF &delta)
{
    switch (m_state) {
    case Pressed:
        m_timeStamp = QTime::currentTime();
        break;
    case Scroll:
        stopTicker();
        m_timeStamp = QTime::currentTime();
        break;
    default:
        break;
    }
    setOffset(offset() - delta);
}

void Scroller::scrollTo(const QPointF &delta)
{
    // Starts scrolling, if at least m_timeTresholdMin msecs has gone since timestamp
    // current time is no more than m_timeTresholdMax from timestamp

    if ((m_timeStamp.elapsed() > m_timeTresholdMin) && (m_timeStamp.msecsTo(QTime::currentTime()) < m_timeTresholdMax)) {
        // Release was quick enough. Start scrolling.
        qreal interval = 25;
        qreal time = m_timeStamp.msecsTo(QTime::currentTime());
        if (qFuzzyCompare(time, 0)) {
            m_speed = delta / 5;
        } else {
            m_speed = delta * interval / time;
        }

        qreal fraction = qMax(qAbs(m_speed.x()), qAbs(m_speed.y()));

        if (!qFuzzyCompare(fraction, 0)) {
            m_fraction.setX(qAbs(m_speed.x() / fraction));
            m_fraction.setY(qAbs(m_speed.y() / fraction));
        } else {
            m_fraction.setX(1);
            m_fraction.setY(1);
        }
        startTicker(interval);
    } else {
        stopTicker();   // Stop ticker, if one is running.
    }
}

void Scroller::handleMousePressEvent(QGraphicsSceneMouseEvent *event)
{
    stopTicker();
    m_pressPos = event->screenPos();
    m_lastPos = m_pressPos;
    m_state = Pressed;
    event->accept();
}

void Scroller::handleMouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF delta = event->screenPos() - m_lastPos;

    switch (m_state) {
    case Pressed: {
        // calculate treshold. If enough, change to move state and send out move deltas.
        if (qAbs(delta.x()) > m_treshold || qAbs(delta.y()) > m_treshold) {
            m_lastPos = event->screenPos();
            move(delta);
            m_state = Move;
        }
        event->accept();
        break;
    }
    case Move: {
        m_lastPos = event->screenPos();
        move(delta);
        event->accept();
        break;
    }
    case Idle:
    default: {
        event->ignore();
        break;
    }
    }
}

void Scroller::handleMouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    switch (m_state) {
    case Move:
    {
        scrollTo(m_lastPos - m_pressPos);
        event->accept();
        break;
    }
    default:
    {
        m_state = Idle;
        event->ignore();
        break;
    }
    }
}

void Scroller::startTicker(int interval)
{
    m_state = Scroll;
    m_ticker.start(interval);
}

void Scroller::stopTicker()
{
    m_state = Idle;
    m_ticker.stop();
}

void Scroller::scrollTick()
{
    switch (m_state) {
    case Scroll:
        lowerSpeed(m_speed);
        setOffset(offset() - m_speed);
        if (m_speed == QPointF(0, 0)) {
            m_state = Idle;
            m_ticker.stop();
        }
        break;
    default:
        qWarning() << __FUNCTION__ << "Scroller unexpected state" << m_state;
        m_ticker.stop();
        m_state = Idle;
        break;
    }
}

void Scroller::lowerSpeed(QPointF &speed, qreal maxSpeed)
{
    qreal x = qBound(-maxSpeed, speed.x(), maxSpeed);
    qreal y = qBound(-maxSpeed, speed.y(), maxSpeed);

    x = (x == 0) ? x :
        (x > 0) ? qMax(qreal(0), x - m_fraction.x()) : qMin(qreal(0), x + m_fraction.x());
    y = (y == 0) ? y :
        (y > 0) ? qMax(qreal(0), y - m_fraction.y()) : qMin(qreal(0), y + m_fraction.y());
    speed.setX(x);
    speed.setY(y);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScrollTicker::ScrollTicker(Scroller *scroller, QObject *parent)
    : QObject(parent),
      m_scroller(scroller)
{

}

void ScrollTicker::start(int interval)
{
    if (!m_timer.isActive())
        m_timer.start(interval, this);
}

void ScrollTicker::stop()
{
    m_timer.stop();
}

void ScrollTicker::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    m_scroller->scrollTick();
}

#include "moc_scroller_p.cpp"

QT_CHARTS_END_NAMESPACE
