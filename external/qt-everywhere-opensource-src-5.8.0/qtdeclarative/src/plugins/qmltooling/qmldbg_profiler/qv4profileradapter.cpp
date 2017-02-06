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

#include "qv4profileradapter.h"
#include "qqmlprofilerservice.h"

QT_BEGIN_NAMESPACE

QV4ProfilerAdapter::QV4ProfilerAdapter(QQmlProfilerService *service, QV4::ExecutionEngine *engine) :
    m_functionCallPos(0), m_memoryPos(0)
{
    setService(service);
    engine->setProfiler(new QV4::Profiling::Profiler(engine));
    connect(this, &QQmlAbstractProfilerAdapter::profilingEnabled,
            this, &QV4ProfilerAdapter::forwardEnabled);
    connect(this, &QQmlAbstractProfilerAdapter::profilingEnabledWhileWaiting,
            this, &QV4ProfilerAdapter::forwardEnabledWhileWaiting, Qt::DirectConnection);
    connect(this, &QV4ProfilerAdapter::v4ProfilingEnabled,
            engine->profiler(), &QV4::Profiling::Profiler::startProfiling);
    connect(this, &QV4ProfilerAdapter::v4ProfilingEnabledWhileWaiting,
            engine->profiler(), &QV4::Profiling::Profiler::startProfiling, Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::profilingDisabled,
            engine->profiler(), &QV4::Profiling::Profiler::stopProfiling);
    connect(this, &QQmlAbstractProfilerAdapter::profilingDisabledWhileWaiting,
            engine->profiler(), &QV4::Profiling::Profiler::stopProfiling,
            Qt::DirectConnection);
    connect(this, &QQmlAbstractProfilerAdapter::dataRequested,
            engine->profiler(), &QV4::Profiling::Profiler::reportData);
    connect(this, &QQmlAbstractProfilerAdapter::referenceTimeKnown,
            engine->profiler(), &QV4::Profiling::Profiler::setTimer);
    connect(engine->profiler(), &QV4::Profiling::Profiler::dataReady,
            this, &QV4ProfilerAdapter::receiveData);
}

qint64 QV4ProfilerAdapter::appendMemoryEvents(qint64 until, QList<QByteArray> &messages,
                                              QQmlDebugPacket &d)
{
    // Make it const, so that we cannot accidentally detach it.
    const QVector<QV4::Profiling::MemoryAllocationProperties> &memoryData = m_memoryData;

    while (memoryData.length() > m_memoryPos && memoryData[m_memoryPos].timestamp <= until) {
        const QV4::Profiling::MemoryAllocationProperties &props = memoryData[m_memoryPos];
        d << props.timestamp << int(MemoryAllocation) << int(props.type) << props.size;
        ++m_memoryPos;
        messages.append(d.squeezedData());
        d.clear();
    }
    return memoryData.length() == m_memoryPos ? -1 : memoryData[m_memoryPos].timestamp;
}

qint64 QV4ProfilerAdapter::finalizeMessages(qint64 until, QList<QByteArray> &messages,
                                            qint64 callNext, QQmlDebugPacket &d)
{
    if (callNext == -1) {
        m_functionLocations.clear();
        m_functionCallData.clear();
        m_functionCallPos = 0;
    }

    qint64 memoryNext = appendMemoryEvents(until, messages, d);

    if (memoryNext == -1) {
        m_memoryData.clear();
        m_memoryPos = 0;
        return callNext;
    }

    return callNext == -1 ? memoryNext : qMin(callNext, memoryNext);
}

qint64 QV4ProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages,
                                        bool trackLocations)
{
    QQmlDebugPacket d;

    // Make it const, so that we cannot accidentally detach it.
    const QVector<QV4::Profiling::FunctionCallProperties> &functionCallData = m_functionCallData;

    while (true) {
        while (!m_stack.isEmpty() &&
               (m_functionCallPos == functionCallData.length() ||
                m_stack.top() <= functionCallData[m_functionCallPos].start)) {
            if (m_stack.top() > until || messages.length() > s_numMessagesPerBatch)
                return finalizeMessages(until, messages, m_stack.top(), d);

            appendMemoryEvents(m_stack.top(), messages, d);
            d << m_stack.pop() << int(RangeEnd) << int(Javascript);
            messages.append(d.squeezedData());
            d.clear();
        }
        while (m_functionCallPos != functionCallData.length() &&
               (m_stack.empty() || functionCallData[m_functionCallPos].start < m_stack.top())) {
            const QV4::Profiling::FunctionCallProperties &props =
                    functionCallData[m_functionCallPos];
            if (props.start > until || messages.length() > s_numMessagesPerBatch)
                return finalizeMessages(until, messages, props.start, d);

            appendMemoryEvents(props.start, messages, d);
            auto location = m_functionLocations.find(props.id);

            d << props.start << int(RangeStart) << int(Javascript);
            if (trackLocations)
                d << static_cast<qint64>(props.id);
            if (location != m_functionLocations.end()) {
                messages.push_back(d.squeezedData());
                d.clear();
                d << props.start << int(RangeLocation) << int(Javascript) << location->file << location->line
                  << location->column;
                if (trackLocations)
                    d << static_cast<qint64>(props.id);
                messages.push_back(d.squeezedData());
                d.clear();
                d << props.start << int(RangeData) << int(Javascript) << location->name;

                if (trackLocations) {
                    d << static_cast<qint64>(props.id);
                    m_functionLocations.erase(location);
                }
            }
            messages.push_back(d.squeezedData());
            d.clear();
            m_stack.push(props.end);
            ++m_functionCallPos;
        }
        if (m_stack.empty() && m_functionCallPos == functionCallData.length())
            return finalizeMessages(until, messages, -1, d);
    }
}

void QV4ProfilerAdapter::receiveData(
        const QV4::Profiling::FunctionLocationHash &locations,
        const QVector<QV4::Profiling::FunctionCallProperties> &functionCallData,
        const QVector<QV4::Profiling::MemoryAllocationProperties> &memoryData)
{
    // In rare cases it could be that another flush or stop event is processed while data from
    // the previous one is still pending. In that case we just append the data.
    if (m_functionLocations.isEmpty())
        m_functionLocations = locations;
    else
        m_functionLocations.unite(locations);

    if (m_functionCallData.isEmpty())
        m_functionCallData = functionCallData;
    else
        m_functionCallData.append(functionCallData);

    if (m_memoryData.isEmpty())
        m_memoryData = memoryData;
    else
        m_memoryData.append(memoryData);

    service->dataReady(this);
}

quint64 QV4ProfilerAdapter::translateFeatures(quint64 qmlFeatures)
{
    quint64 v4Features = 0;
    const quint64 one = 1;
    if (qmlFeatures & (one << ProfileJavaScript))
        v4Features |= (one << QV4::Profiling::FeatureFunctionCall);
    if (qmlFeatures & (one << ProfileMemory))
        v4Features |= (one << QV4::Profiling::FeatureMemoryAllocation);
    return v4Features;
}

void QV4ProfilerAdapter::forwardEnabled(quint64 features)
{
    emit v4ProfilingEnabled(translateFeatures(features));
}

void QV4ProfilerAdapter::forwardEnabledWhileWaiting(quint64 features)
{
    emit v4ProfilingEnabledWhileWaiting(translateFeatures(features));
}

QT_END_NAMESPACE
