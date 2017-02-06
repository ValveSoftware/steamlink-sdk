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

#include "qv4debuggeragent.h"
#include "qv4debugservice.h"
#include "qv4datacollector.h"

#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonarray.h>

QT_BEGIN_NAMESPACE

QV4DebuggerAgent::QV4DebuggerAgent(QV4DebugServiceImpl *debugService)
    : m_breakOnThrow(false), m_debugService(debugService)
{}

QV4Debugger *QV4DebuggerAgent::pausedDebugger() const
{
    foreach (QV4Debugger *debugger, m_debuggers) {
        if (debugger->state() == QV4Debugger::Paused)
            return debugger;
    }
    return 0;
}

bool QV4DebuggerAgent::isRunning() const
{
    // "running" means none of the engines are paused.
    return pausedDebugger() == 0;
}

void QV4DebuggerAgent::debuggerPaused(QV4Debugger *debugger, QV4Debugger::PauseReason reason)
{
    Q_UNUSED(reason);

    debugger->collector()->clear();

    QJsonObject event, body, script;
    event.insert(QStringLiteral("type"), QStringLiteral("event"));

    switch (reason) {
    case QV4Debugger::Step:
    case QV4Debugger::PauseRequest:
    case QV4Debugger::BreakPointHit: {
        event.insert(QStringLiteral("event"), QStringLiteral("break"));
        QVector<QV4::StackFrame> frames = debugger->stackTrace(1);
        if (frames.isEmpty())
            break;

        const QV4::StackFrame &topFrame = frames.first();
        body.insert(QStringLiteral("invocationText"), topFrame.function);
        body.insert(QStringLiteral("sourceLine"), topFrame.line - 1);
        if (topFrame.column > 0)
            body.insert(QStringLiteral("sourceColumn"), topFrame.column);
        QJsonArray breakPoints;
        foreach (int breakPointId, breakPointIds(topFrame.source, topFrame.line))
            breakPoints.push_back(breakPointId);
        body.insert(QStringLiteral("breakpoints"), breakPoints);
        script.insert(QStringLiteral("name"), topFrame.source);
    } break;
    case QV4Debugger::Throwing:
        // TODO: complete this!
        event.insert(QStringLiteral("event"), QStringLiteral("exception"));
        break;
    }

    if (!script.isEmpty())
        body.insert(QStringLiteral("script"), script);
    if (!body.isEmpty())
        event.insert(QStringLiteral("body"), body);
    m_debugService->send(event);
}

void QV4DebuggerAgent::addDebugger(QV4Debugger *debugger)
{
    Q_ASSERT(!m_debuggers.contains(debugger));
    m_debuggers << debugger;

    debugger->setBreakOnThrow(m_breakOnThrow);

    for (const BreakPoint &breakPoint : qAsConst(m_breakPoints))
        if (breakPoint.enabled)
            debugger->addBreakPoint(breakPoint.fileName, breakPoint.lineNr, breakPoint.condition);

    connect(debugger, &QObject::destroyed, this, &QV4DebuggerAgent::handleDebuggerDeleted);
    connect(debugger, &QV4Debugger::debuggerPaused, this, &QV4DebuggerAgent::debuggerPaused,
            Qt::QueuedConnection);
}

void QV4DebuggerAgent::removeDebugger(QV4Debugger *debugger)
{
    m_debuggers.removeAll(debugger);
    disconnect(debugger, &QObject::destroyed, this, &QV4DebuggerAgent::handleDebuggerDeleted);
    disconnect(debugger, &QV4Debugger::debuggerPaused, this, &QV4DebuggerAgent::debuggerPaused);
}

const QList<QV4Debugger *> &QV4DebuggerAgent::debuggers()
{
    return m_debuggers;
}

void QV4DebuggerAgent::handleDebuggerDeleted(QObject *debugger)
{
    m_debuggers.removeAll(static_cast<QV4Debugger *>(debugger));
}

void QV4DebuggerAgent::pause(QV4Debugger *debugger) const
{
    debugger->pause();
}

void QV4DebuggerAgent::pauseAll() const
{
    foreach (QV4Debugger *debugger, m_debuggers)
        pause(debugger);
}

void QV4DebuggerAgent::resumeAll() const
{
    foreach (QV4Debugger *debugger, m_debuggers)
        if (debugger->state() == QV4Debugger::Paused)
            debugger->resume(QV4Debugger::FullThrottle);
}

int QV4DebuggerAgent::addBreakPoint(const QString &fileName, int lineNumber, bool enabled, const QString &condition)
{
    if (enabled)
        foreach (QV4Debugger *debugger, m_debuggers)
            debugger->addBreakPoint(fileName, lineNumber, condition);

    int id = m_breakPoints.size();
    m_breakPoints.insert(id, BreakPoint(fileName, lineNumber, enabled, condition));
    return id;
}

void QV4DebuggerAgent::removeBreakPoint(int id)
{
    BreakPoint breakPoint = m_breakPoints.value(id);
    if (!breakPoint.isValid())
        return;

    m_breakPoints.remove(id);

    if (breakPoint.enabled)
        foreach (QV4Debugger *debugger, m_debuggers)
            debugger->removeBreakPoint(breakPoint.fileName, breakPoint.lineNr);
}

void QV4DebuggerAgent::removeAllBreakPoints()
{
    for (auto it = m_breakPoints.keyBegin(), end = m_breakPoints.keyEnd(); it != end; ++it)
        removeBreakPoint(*it);
}

void QV4DebuggerAgent::enableBreakPoint(int id, bool onoff)
{
    BreakPoint &breakPoint = m_breakPoints[id];
    if (!breakPoint.isValid() || breakPoint.enabled == onoff)
        return;
    breakPoint.enabled = onoff;

    foreach (QV4Debugger *debugger, m_debuggers) {
        if (onoff)
            debugger->addBreakPoint(breakPoint.fileName, breakPoint.lineNr, breakPoint.condition);
        else
            debugger->removeBreakPoint(breakPoint.fileName, breakPoint.lineNr);
    }
}

QList<int> QV4DebuggerAgent::breakPointIds(const QString &fileName, int lineNumber) const
{
    QList<int> ids;

    for (QHash<int, BreakPoint>::const_iterator i = m_breakPoints.begin(), ei = m_breakPoints.end(); i != ei; ++i)
        if (i->lineNr == lineNumber && fileName.endsWith(i->fileName))
            ids.push_back(i.key());

    return ids;
}

void QV4DebuggerAgent::setBreakOnThrow(bool onoff)
{
    if (onoff != m_breakOnThrow) {
        m_breakOnThrow = onoff;
        foreach (QV4Debugger *debugger, m_debuggers)
            debugger->setBreakOnThrow(onoff);
    }
}

void QV4DebuggerAgent::clearAllPauseRequests()
{
    foreach (QV4Debugger *debugger, m_debuggers)
        debugger->clearPauseRequest();
}

QT_END_NAMESPACE
