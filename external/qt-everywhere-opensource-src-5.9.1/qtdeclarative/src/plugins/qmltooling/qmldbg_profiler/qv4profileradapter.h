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

#ifndef QV4PROFILERADAPTER_P_H
#define QV4PROFILERADAPTER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qv4profiling_p.h>
#include <private/qqmlabstractprofileradapter_p.h>
#include "qqmldebugpacket.h"

#include <QStack>
#include <QList>

QT_BEGIN_NAMESPACE

class QQmlProfilerService;
class QV4ProfilerAdapter : public QQmlAbstractProfilerAdapter {
    Q_OBJECT

public:
    QV4ProfilerAdapter(QQmlProfilerService *service, QV4::ExecutionEngine *engine);

    virtual qint64 sendMessages(qint64 until, QList<QByteArray> &messages,
                                bool trackLocations) override;

    void receiveData(const QV4::Profiling::FunctionLocationHash &,
                     const QVector<QV4::Profiling::FunctionCallProperties> &,
                     const QVector<QV4::Profiling::MemoryAllocationProperties> &);

signals:
    void v4ProfilingEnabled(quint64 v4Features);
    void v4ProfilingEnabledWhileWaiting(quint64 v4Features);

private:
    QV4::Profiling::FunctionLocationHash m_functionLocations;
    QVector<QV4::Profiling::FunctionCallProperties> m_functionCallData;
    QVector<QV4::Profiling::MemoryAllocationProperties> m_memoryData;
    int m_functionCallPos;
    int m_memoryPos;
    QStack<qint64> m_stack;
    qint64 appendMemoryEvents(qint64 until, QList<QByteArray> &messages, QQmlDebugPacket &d);
    qint64 finalizeMessages(qint64 until, QList<QByteArray> &messages, qint64 callNext,
                            QQmlDebugPacket &d);
    void forwardEnabled(quint64 features);
    void forwardEnabledWhileWaiting(quint64 features);

    static quint64 translateFeatures(quint64 qmlFeatures);
};

QT_END_NAMESPACE

#endif // QV4PROFILERADAPTER_P_H
