/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "scriptbreakpointmanager.h"

ScriptBreakpointManager::ScriptBreakpointManager()
{
}

ScriptBreakpointManager::~ScriptBreakpointManager()
{
}

bool ScriptBreakpointManager::hasBreakpoints() const
{
    return !breakpoints.isEmpty();
}

int ScriptBreakpointManager::setBreakpoint(const QString &fileName, int lineNumber)
{
    breakpoints.append(ScriptBreakpointInfo(fileName, lineNumber));
    return breakpoints.size() - 1;
}

int ScriptBreakpointManager::setBreakpoint(const QString &functionName, const QString &fileName)
{
    breakpoints.append(ScriptBreakpointInfo(functionName, fileName));
    return breakpoints.size() - 1;
}

int ScriptBreakpointManager::setBreakpoint(const QScriptValue &function)
{
    breakpoints.append(ScriptBreakpointInfo(function));
    return breakpoints.size() - 1;
}

void ScriptBreakpointManager::removeBreakpoint(int id)
{
    if (id >= 0 && id < breakpoints.size())
        breakpoints[id] = ScriptBreakpointInfo();
}

int ScriptBreakpointManager::findBreakpoint(const QString &fileName, int lineNumber) const
{
    for (int i = 0; i < breakpoints.size(); ++i) {
        const ScriptBreakpointInfo &brk = breakpoints.at(i);
        if (brk.type != ScriptBreakpointInfo::File)
            continue;
        if (brk.fileName == fileName && brk.lineNumber == lineNumber)
            return i;
    }
    return -1;
}

int ScriptBreakpointManager::findBreakpoint(const QString &functionName, const QString &fileName) const
{
    for (int i = 0; i < breakpoints.size(); ++i) {
        const ScriptBreakpointInfo &brk = breakpoints.at(i);
        if (brk.type != ScriptBreakpointInfo::FunctionName)
            continue;
        if (brk.functionName == functionName && brk.fileName == fileName)
            return i;
    }
    return -1;
}

int ScriptBreakpointManager::findBreakpoint(const QScriptValue &function) const
{
    for (int i = 0; i < breakpoints.size(); ++i) {
        const ScriptBreakpointInfo &brk = breakpoints.at(i);
        if (brk.type != ScriptBreakpointInfo::Function)
            continue;
        if (brk.function.strictlyEquals(function))
            return i;
    }
    return -1;
}

bool ScriptBreakpointManager::isBreakpointEnabled(int id) const
{
    return breakpoints.value(id).enabled;
}

void ScriptBreakpointManager::setBreakpointEnabled(int id, bool enabled)
{
    if (id >= 0 && id < breakpoints.size())
        breakpoints[id].enabled = enabled;
}

QString ScriptBreakpointManager::breakpointCondition(int id) const
{
    return breakpoints.value(id).condition;
}

void ScriptBreakpointManager::setBreakpointCondition(int id, const QString &expression)
{
    if (id >= 0 && id < breakpoints.size())
        breakpoints[id].condition = expression;
}

int ScriptBreakpointManager::breakpointIgnoreCount(int id) const
{
    return breakpoints.value(id).ignoreCount;
}

void ScriptBreakpointManager::setBreakpointIgnoreCount(int id, int ignoreCount)
{
    if (id >= 0 && id < breakpoints.size())
        breakpoints[id].ignoreCount = ignoreCount;
}

bool ScriptBreakpointManager::isBreakpointSingleShot(int id) const
{
    return breakpoints.value(id).singleShot;
}

void ScriptBreakpointManager::setBreakpointSingleShot(int id, bool singleShot)
{
    if (id >= 0 && id < breakpoints.size())
        breakpoints[id].singleShot = singleShot;
}
