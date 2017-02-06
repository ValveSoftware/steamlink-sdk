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

#include "qv4profiling_p.h"
#include <private/qv4mm_p.h>
#include <private/qv4string_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {
namespace Profiling {

FunctionLocation FunctionCall::resolveLocation() const
{
    return FunctionLocation(m_function->name()->toQString(),
                            m_function->compilationUnit->fileName(),
                            m_function->compiledFunction->location.line,
                            m_function->compiledFunction->location.column);
}

FunctionCallProperties FunctionCall::properties() const
{
    FunctionCallProperties props = {
        m_start,
        m_end,
        reinterpret_cast<quintptr>(m_function)
    };
    return props;
}

Profiler::Profiler(QV4::ExecutionEngine *engine) : featuresEnabled(0), m_engine(engine)
{
    static const int metatypes[] = {
        qRegisterMetaType<QVector<QV4::Profiling::FunctionCallProperties> >(),
        qRegisterMetaType<QVector<QV4::Profiling::MemoryAllocationProperties> >(),
        qRegisterMetaType<FunctionLocationHash>()
    };
    Q_UNUSED(metatypes);
    m_timer.start();
}

void Profiler::stopProfiling()
{
    featuresEnabled = 0;
    reportData(true);
    m_sentLocations.clear();
}

bool operator<(const FunctionCall &call1, const FunctionCall &call2)
{
    return call1.m_start < call2.m_start ||
            (call1.m_start == call2.m_start && (call1.m_end < call2.m_end ||
            (call1.m_end == call2.m_end && call1.m_function < call2.m_function)));
}

void Profiler::reportData(bool trackLocations)
{
    std::sort(m_data.begin(), m_data.end());
    QVector<FunctionCallProperties> properties;
    FunctionLocationHash locations;
    properties.reserve(m_data.size());

    foreach (const FunctionCall &call, m_data) {
        properties.append(call.properties());
        Function *function = call.function();
        SentMarker &marker = m_sentLocations[reinterpret_cast<quintptr>(function)];
        if (!trackLocations || !marker.isValid()) {
            FunctionLocation &location = locations[properties.constLast().id];
            if (!location.isValid())
                location = call.resolveLocation();
            if (trackLocations)
                marker.setFunction(function);
        }
    }

    emit dataReady(locations, properties, m_memory_data);
    m_data.clear();
    m_memory_data.clear();
}

void Profiler::startProfiling(quint64 features)
{
    if (featuresEnabled == 0) {
        if (features & (1 << FeatureMemoryAllocation)) {
            qint64 timestamp = m_timer.nsecsElapsed();
            MemoryAllocationProperties heap = {timestamp,
                                               (qint64)m_engine->memoryManager->getAllocatedMem(),
                                               HeapPage};
            m_memory_data.append(heap);
            MemoryAllocationProperties small = {timestamp,
                                                (qint64)m_engine->memoryManager->getUsedMem(),
                                                SmallItem};
            m_memory_data.append(small);
            MemoryAllocationProperties large = {timestamp,
                                                (qint64)m_engine->memoryManager->getLargeItemsMem(),
                                                LargeItem};
            m_memory_data.append(large);
        }

        featuresEnabled = features;
    }
}

} // namespace Profiling
} // namespace QV4

QT_END_NAMESPACE
