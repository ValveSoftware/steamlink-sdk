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

#ifndef QV4DATACOLLECTOR_H
#define QV4DATACOLLECTOR_H

#include <private/qv4engine_p.h>
#include <private/qv4persistent_p.h>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

QT_BEGIN_NAMESPACE

class QV4Debugger;
class QV4DataCollector
{
public:
    typedef uint Ref;
    typedef QVector<uint> Refs;

    static QV4::Heap::CallContext *findScope(QV4::ExecutionContext *ctxt, int scope);
    static int encodeScopeType(QV4::Heap::ExecutionContext::ContextType scopeType);

    QVector<QV4::Heap::ExecutionContext::ContextType> getScopeTypes(int frame);
    QV4::CallContext *findContext(int frame);

    QV4DataCollector(QV4::ExecutionEngine *engine);

    Ref collect(const QV4::ScopedValue &value);
    Ref addFunctionRef(const QString &functionName);
    Ref addScriptRef(const QString &scriptName);

    bool isValidRef(Ref ref) const;
    QJsonObject lookupRef(Ref ref);

    bool collectScope(QJsonObject *dict, int frameNr, int scopeNr);
    QJsonObject buildFrame(const QV4::StackFrame &stackFrame, int frameNr);

    QV4::ExecutionEngine *engine() const { return m_engine; }
    QJsonArray flushCollectedRefs();
    void clear();

private:
    Ref addRef(QV4::Value value, bool deduplicate = true);
    QV4::ReturnedValue getValue(Ref ref);
    bool lookupSpecialRef(Ref ref, QJsonObject *dict);

    QJsonArray collectProperties(const QV4::Object *object);
    QJsonObject collectAsJson(const QString &name, const QV4::ScopedValue &value);
    void collectArgumentsInContext();

    QV4::ExecutionEngine *m_engine;
    Refs m_collectedRefs;
    QV4::PersistentValue m_values;
    typedef QHash<Ref, QJsonObject> SpecialRefs;
    SpecialRefs m_specialRefs;
};

QT_END_NAMESPACE

#endif // QV4DATACOLLECTOR_H
