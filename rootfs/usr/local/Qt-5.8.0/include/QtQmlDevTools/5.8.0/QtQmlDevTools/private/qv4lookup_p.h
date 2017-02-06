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
#ifndef QV4LOOKUP_H
#define QV4LOOKUP_H

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

#include "qv4global_p.h"
#include "qv4runtime_p.h"
#include "qv4engine_p.h"
#include "qv4context_p.h"
#include "qv4object_p.h"
#include "qv4internalclass_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Lookup {
    enum { Size = 4 };
    union {
        ReturnedValue (*indexedGetter)(Lookup *l, const Value &object, const Value &index);
        void (*indexedSetter)(Lookup *l, const Value &object, const Value &index, const Value &v);
        ReturnedValue (*getter)(Lookup *l, ExecutionEngine *engine, const Value &object);
        ReturnedValue (*globalGetter)(Lookup *l, ExecutionEngine *engine);
        void (*setter)(Lookup *l, ExecutionEngine *engine, Value &object, const Value &v);
    };
    union {
        ExecutionEngine *engine;
        InternalClass *classList[Size];
        struct {
            void *dummy0;
            void *dummy1;
            Object *proto;
            unsigned type;
        };
    };
    union {
        int level;
        uint index2;
    };
    uint index;
    uint nameIndex;

    static ReturnedValue indexedGetterGeneric(Lookup *l, const Value &object, const Value &index);
    static ReturnedValue indexedGetterFallback(Lookup *l, const Value &object, const Value &index);
    static ReturnedValue indexedGetterObjectInt(Lookup *l, const Value &object, const Value &index);

    static void indexedSetterGeneric(Lookup *l, const Value &object, const Value &index, const Value &v);
    static void indexedSetterFallback(Lookup *l, const Value &object, const Value &index, const Value &value);
    static void indexedSetterObjectInt(Lookup *l, const Value &object, const Value &index, const Value &v);

    static ReturnedValue getterGeneric(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterTwoClasses(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterFallback(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue getter0(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter2(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0getter0(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter0getter1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getter1getter1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterAccessor0(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterAccessor1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue getterAccessor2(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue primitiveGetter0(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue primitiveGetter1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue primitiveGetterAccessor0(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue primitiveGetterAccessor1(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue stringLengthGetter(Lookup *l, ExecutionEngine *engine, const Value &object);
    static ReturnedValue arrayLengthGetter(Lookup *l, ExecutionEngine *engine, const Value &object);

    static ReturnedValue globalGetterGeneric(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetter0(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetter1(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetter2(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetterAccessor0(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetterAccessor1(Lookup *l, ExecutionEngine *engine);
    static ReturnedValue globalGetterAccessor2(Lookup *l, ExecutionEngine *engine);

    static void setterGeneric(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setterTwoClasses(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setterFallback(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setter0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setterInsert0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setterInsert1(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setterInsert2(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);
    static void setter0setter0(Lookup *l, ExecutionEngine *engine, Value &object, const Value &value);

    ReturnedValue lookup(const Value &thisObject, Object *obj, PropertyAttributes *attrs);
    ReturnedValue lookup(const Object *obj, PropertyAttributes *attrs);

};

}

QT_END_NAMESPACE

#endif
