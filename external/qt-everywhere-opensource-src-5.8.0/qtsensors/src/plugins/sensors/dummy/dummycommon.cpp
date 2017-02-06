/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "dummycommon.h"

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
// WINCE has <time.h> but using clock() gives a link error because
// the function isn't actually implemented.
#else
#include <time.h>
#ifdef Q_OS_MAC
#include <mach/mach_time.h>
#endif
#endif

dummycommon::dummycommon(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_timerid(0)
{
}

void dummycommon::start()
{
    if (m_timerid)
        return;

    int dataRate = sensor()->dataRate();
    if (dataRate == 0) {
        if (sensor()->availableDataRates().count())
            // Use the first available rate when -1 is chosen
            dataRate = sensor()->availableDataRates().first().first;
        else
            dataRate = 1;
    }

    int interval = 1000 / dataRate;

    if (interval)
        m_timerid = startTimer(interval);
}

void dummycommon::stop()
{
    if (m_timerid) {
        killTimer(m_timerid);
        m_timerid = 0;
    }
}

void dummycommon::timerEvent(QTimerEvent * /*event*/)
{
    poll();
}

#ifdef Q_OS_MAC
//taken from qelapsedtimer_mac.cpp
static mach_timebase_info_data_t info = {0,0};
static qint64 absoluteToNSecs(qint64 cpuTime)
{
    if (info.denom == 0)
        mach_timebase_info(&info);
    qint64 nsecs = cpuTime * info.numer / info.denom;
    return nsecs;
}
#elif defined(Q_OS_WIN)
// Obtain a time stamp from the performance counter,
// default to tick count.
static quint64 windowsTimeStamp()
{
    static bool hasFrequency =  false;
    static quint64 frequency = 0;
    if (!hasFrequency) {
        LARGE_INTEGER frequencyLI;
        hasFrequency = true;
        QueryPerformanceFrequency(&frequencyLI);
        frequency = frequencyLI.QuadPart;
    }

    if (frequency) { // Microseconds.
        LARGE_INTEGER counterLI;
        if (QueryPerformanceCounter(&counterLI))
            return 1000000 * counterLI.QuadPart / frequency;
    }
#ifndef Q_OS_WINRT
    return GetTickCount();
#else
    return GetTickCount64();
#endif
}
#endif

quint64 dummycommon::getTimestamp()
{
#if defined(Q_OS_WIN)
    return windowsTimeStamp();
#elif defined(Q_OS_WINCE)
    //d This implementation is based on code found here:
    // http://social.msdn.microsoft.com/Forums/en/vssmartdevicesnative/thread/74870c6c-76c5-454c-8533-812cfca585f8
    HANDLE currentThread = GetCurrentThread();
    FILETIME creationTime, exitTime, kernalTime, userTime;
    GetThreadTimes(currentThread, &creationTime, &exitTime, &kernalTime, &userTime);

    ULARGE_INTEGER uli;
    uli.LowPart = userTime.dwLowDateTime;
    uli.HighPart = userTime.dwHighDateTime;
    ULONGLONG systemTimeInMS = uli.QuadPart/10000;
    return static_cast<quint64>(systemTimeInMS);
#elif defined(Q_OS_MAC)
    uint64_t cpu_time = mach_absolute_time();
    uint64_t nsecs = absoluteToNSecs(cpu_time);

    quint64 result = (nsecs * 0.001); //scale to microseconds
    return result;
#else
    struct timespec tv;
    int ok;
    ok = clock_gettime(CLOCK_MONOTONIC, &tv);
    Q_ASSERT(ok == 0);
    Q_UNUSED(ok);

    quint64 result = (tv.tv_sec * 1000000ULL) + (tv.tv_nsec * 0.001); // scale to microseconds
    return result;
#endif
}

