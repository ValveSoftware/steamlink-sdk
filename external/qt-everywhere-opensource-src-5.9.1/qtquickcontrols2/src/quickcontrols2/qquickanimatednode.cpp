/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
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

#include "qquickanimatednode_p.h"

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>

// based on qtdeclarative/examples/quick/scenegraph/threadedanimation

QT_BEGIN_NAMESPACE

QQuickAnimatedNode::QQuickAnimatedNode(QQuickItem *target)
    : m_running(false),
      m_duration(0),
      m_loopCount(1),
      m_currentTime(0),
      m_currentLoop(0),
      m_window(target->window())
{
}

bool QQuickAnimatedNode::isRunning() const
{
    return m_running;
}

int QQuickAnimatedNode::currentTime() const
{
    int time = m_currentTime;
    if (m_running)
        time += m_timer.elapsed();
    return time;
}

void QQuickAnimatedNode::setCurrentTime(int time)
{
    m_currentTime = time;
    m_timer.restart();
}

int QQuickAnimatedNode::duration() const
{
    return m_duration;
}

void QQuickAnimatedNode::setDuration(int duration)
{
    m_duration = duration;
}

int QQuickAnimatedNode::loopCount() const
{
    return m_loopCount;
}

void QQuickAnimatedNode::setLoopCount(int count)
{
    m_loopCount = count;
}

void QQuickAnimatedNode::sync(QQuickItem *target)
{
    Q_UNUSED(target);
}

QQuickWindow *QQuickAnimatedNode::window() const
{
    return m_window;
}

void QQuickAnimatedNode::start(int duration)
{
    if (m_running)
        return;

    m_running = true;
    m_currentLoop = 0;
    m_timer.restart();
    if (duration > 0)
        m_duration = duration;
    connect(m_window, &QQuickWindow::beforeRendering, this, &QQuickAnimatedNode::advance);
    connect(m_window, &QQuickWindow::frameSwapped, this, &QQuickAnimatedNode::update);
    emit started();
}

void QQuickAnimatedNode::restart()
{
    stop();
    start();
}

void QQuickAnimatedNode::stop()
{
    if (!m_running)
        return;

    m_running = false;
    disconnect(m_window, &QQuickWindow::beforeRendering, this, &QQuickAnimatedNode::advance);
    disconnect(m_window, &QQuickWindow::frameSwapped, this, &QQuickAnimatedNode::update);
    emit stopped();
}

void QQuickAnimatedNode::updateCurrentTime(int time)
{
    Q_UNUSED(time);
}

void QQuickAnimatedNode::advance()
{
    int time = currentTime();
    if (time > m_duration) {
        time = 0;
        setCurrentTime(0);

        if (m_loopCount > 0 && ++m_currentLoop >= m_loopCount) {
            time = m_duration; // complete
            stop();
        }
    }
    updateCurrentTime(time);
}

void QQuickAnimatedNode::update()
{
    if (m_running)
        m_window->update();
}

QT_END_NAMESPACE
