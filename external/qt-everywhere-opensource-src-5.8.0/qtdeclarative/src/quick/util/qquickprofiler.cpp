/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qquickprofiler_p.h"

#include <QtQml/private/qqmlabstractprofileradapter_p.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

// instance will be set, unset in constructor. Allows static methods to be inlined.
QQuickProfiler *QQuickProfiler::s_instance = 0;
quint64 QQuickProfiler::featuresEnabled = 0;

void QQuickProfiler::initialize(QObject *parent)
{
    Q_ASSERT(s_instance == 0);
    s_instance = new QQuickProfiler(parent);
}

void animationTimerCallback(qint64 delta)
{
    Q_QUICK_PROFILE(QQuickProfiler::ProfileAnimations, animationFrame(delta,
            QThread::currentThread() == QCoreApplication::instance()->thread() ?
            QQuickProfiler::GuiThread : QQuickProfiler::RenderThread));
}

void QQuickProfiler::registerAnimationCallback()
{
    QUnifiedTimer::instance()->registerProfilerCallback(&animationTimerCallback);
}

class CallbackRegistrationHelper : public QObject {
    Q_OBJECT
public:
    void registerAnimationTimerCallback()
    {
        QQuickProfiler::registerAnimationCallback();
        delete this;
    }
};

#include "qquickprofiler.moc"

QQuickProfiler::QQuickProfiler(QObject *parent) : QObject(parent)
{
    // This is safe because at this point the m_instance isn't initialized, yet.
    m_timer.start();
    CallbackRegistrationHelper *helper = new CallbackRegistrationHelper; // will delete itself
    helper->moveToThread(QCoreApplication::instance()->thread());

    // Queue the signal to have the animation timer registration run in the right thread;
    QObject signalSource;
    connect(&signalSource, &QObject::destroyed,
            helper, &CallbackRegistrationHelper::registerAnimationTimerCallback,
            Qt::QueuedConnection);
}

QQuickProfiler::~QQuickProfiler()
{
    QMutexLocker lock(&m_dataMutex);
    featuresEnabled = 0;
    s_instance = 0;
}

void QQuickProfiler::startProfilingImpl(quint64 features)
{
    QMutexLocker lock(&m_dataMutex);
    featuresEnabled = features;
}

void QQuickProfiler::stopProfilingImpl()
{
    QMutexLocker lock(&m_dataMutex);
    featuresEnabled = 0;
    emit dataReady(m_data);
    m_data.clear();
}

void QQuickProfiler::reportDataImpl(bool trackLocations)
{
    Q_UNUSED(trackLocations);
    QMutexLocker lock(&m_dataMutex);
    emit dataReady(m_data);
    m_data.clear();
}

void QQuickProfiler::setTimer(const QElapsedTimer &t)
{
    QMutexLocker lock(&m_dataMutex);
    m_timer = t;
}

QT_END_NAMESPACE
