/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4profileradapter_p.h"
#include "qqmlprofilerservice_p.h"
#include "qqmldebugservice_p.h"

QT_BEGIN_NAMESPACE

QV4ProfilerAdapter::QV4ProfilerAdapter(QQmlProfilerService *service, QV4::ExecutionEngine *engine) :
    QQmlAbstractProfilerAdapter(service)
{
    engine->enableProfiler();
    connect(this, SIGNAL(profilingEnabled(quint64)),
            engine->profiler, SLOT(startProfiling(quint64)));
    connect(this, SIGNAL(profilingEnabledWhileWaiting(quint64)),
            engine->profiler, SLOT(startProfiling(quint64)), Qt::DirectConnection);
    connect(this, SIGNAL(profilingDisabled()), engine->profiler, SLOT(stopProfiling()));
    connect(this, SIGNAL(profilingDisabledWhileWaiting()), engine->profiler, SLOT(stopProfiling()),
            Qt::DirectConnection);
    connect(this, SIGNAL(dataRequested()), engine->profiler, SLOT(reportData()));
    connect(this, SIGNAL(referenceTimeKnown(QElapsedTimer)),
            engine->profiler, SLOT(setTimer(QElapsedTimer)));
    connect(engine->profiler, SIGNAL(dataReady(QList<QV4::Profiling::FunctionCallProperties>,
                                               QList<QV4::Profiling::MemoryAllocationProperties>)),
            this, SLOT(receiveData(QList<QV4::Profiling::FunctionCallProperties>,
                                   QList<QV4::Profiling::MemoryAllocationProperties>)));
}

qint64 QV4ProfilerAdapter::appendMemoryEvents(qint64 until, QList<QByteArray> &messages)
{
    QByteArray message;
    while (!memory_data.empty() && memory_data.front().timestamp <= until) {
        QQmlDebugStream d(&message, QIODevice::WriteOnly);
        QV4::Profiling::MemoryAllocationProperties &props = memory_data.front();
        d << props.timestamp << MemoryAllocation << props.type << props.size;
        memory_data.pop_front();
        messages.append(message);
    }
    return memory_data.empty() ? -1 : memory_data.front().timestamp;
}

qint64 QV4ProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages)
{
    QByteArray message;
    while (true) {
        while (!stack.empty() && (data.empty() || stack.top() <= data.front().start)) {
            if (stack.top() > until) {
                qint64 memory_next = appendMemoryEvents(until, messages);
                return memory_next == -1 ? stack.top() : qMin(stack.top(), memory_next);
            }
            appendMemoryEvents(stack.top(), messages);
            QQmlDebugStream d(&message, QIODevice::WriteOnly);
            d << stack.pop() << RangeEnd << Javascript;
            messages.append(message);
        }
        while (!data.empty() && (stack.empty() || data.front().start < stack.top())) {
            const QV4::Profiling::FunctionCallProperties &props = data.front();
            if (props.start > until) {
                qint64 memory_next = appendMemoryEvents(until, messages);
                return memory_next == -1 ? props.start : qMin(props.start, memory_next);
            }
            appendMemoryEvents(props.start, messages);

            QQmlDebugStream d_start(&message, QIODevice::WriteOnly);
            d_start << props.start << RangeStart << Javascript;
            messages.push_back(message);
            message.clear();
            QQmlDebugStream d_location(&message, QIODevice::WriteOnly);
            d_location << props.start << RangeLocation << Javascript << props.file << props.line
                       << props.column;
            messages.push_back(message);
            message.clear();
            QQmlDebugStream d_data(&message, QIODevice::WriteOnly);
            d_data << props.start << RangeData << Javascript << props.name;
            messages.push_back(message);
            message.clear();
            stack.push(props.end);
            data.pop_front();
        }
        if (stack.empty() && data.empty())
            return appendMemoryEvents(until, messages);
    }
}

void QV4ProfilerAdapter::receiveData(const QList<QV4::Profiling::FunctionCallProperties> &new_data,
        const QList<QV4::Profiling::MemoryAllocationProperties> &new_memory_data)
{
    data = new_data;
    memory_data = new_memory_data;
    stack.clear();
    service->dataReady(this);
}

QT_END_NAMESPACE
