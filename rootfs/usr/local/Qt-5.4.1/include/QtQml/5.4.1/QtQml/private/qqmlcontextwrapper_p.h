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

#ifndef QQMLCONTEXTWRAPPER_P_H
#define QQMLCONTEXTWRAPPER_P_H

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

#include <QtCore/qglobal.h>
#include <private/qtqmlglobal_p.h>

#include <private/qv4value_inl_p.h>
#include <private/qv4object_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qv4functionobject_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace CompiledData {
struct Function;
}

struct QQmlIdObjectsArray;

struct Q_QML_EXPORT QmlContextWrapper : Object
{
    struct Data : Object::Data {
        Data(QV8Engine *engine, QQmlContextData *context, QObject *scopeObject, bool ownsContext = false);
        ~Data();
        bool readOnly;
        bool ownsContext;
        bool isNullWrapper;

        QQmlGuardedContextData context;
        QPointer<QObject> scopeObject;
        QQmlIdObjectsArray *idObjectsWrapper;
    };
    V4_OBJECT(Object)

    static ReturnedValue qmlScope(QV8Engine *e, QQmlContextData *ctxt, QObject *scope);
    static ReturnedValue urlScope(QV8Engine *e, const QUrl &);

    static QQmlContextData *callingContext(ExecutionEngine *v4);
    static void takeContextOwnership(const ValueRef qmlglobal);

    inline QObject *getScopeObject() const { return d()->scopeObject; }
    inline QQmlContextData *getContext() const { return d()->context; }
    static QQmlContextData *getContext(const ValueRef value);

    void setReadOnly(bool b) { d()->readOnly = b; }

    static ReturnedValue get(Managed *m, String *name, bool *hasProperty);
    static void put(Managed *m, String *name, const ValueRef value);
    static void destroy(Managed *that);
    static void markObjects(Managed *m, ExecutionEngine *engine);

    static void registerQmlDependencies(ExecutionEngine *context, const CompiledData::Function *compiledFunction);

    ReturnedValue idObjectsArray();
    ReturnedValue qmlSingletonWrapper(QV8Engine *e, String *name);

};

struct QQmlIdObjectsArray : public Object
{
    struct Data : Object::Data {
        Data(ExecutionEngine *engine, QmlContextWrapper *contextWrapper);
        QmlContextWrapper *contextWrapper;
    };
    V4_OBJECT(Object)

    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
    static void markObjects(Managed *that, ExecutionEngine *engine);

};

}

QT_END_NAMESPACE

#endif // QV8CONTEXTWRAPPER_P_H

