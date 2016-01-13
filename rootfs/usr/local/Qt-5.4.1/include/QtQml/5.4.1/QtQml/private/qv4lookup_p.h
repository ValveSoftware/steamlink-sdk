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
#ifndef QV4LOOKUP_H
#define QV4LOOKUP_H

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
        ReturnedValue (*indexedGetter)(Lookup *l, const ValueRef object, const ValueRef index);
        void (*indexedSetter)(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef v);
        ReturnedValue (*getter)(Lookup *l, const ValueRef object);
        ReturnedValue (*globalGetter)(Lookup *l, ExecutionContext *ctx);
        void (*setter)(Lookup *l, const ValueRef object, const ValueRef v);
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
    String *name;

    static ReturnedValue indexedGetterGeneric(Lookup *l, const ValueRef object, const ValueRef index);
    static ReturnedValue indexedGetterFallback(Lookup *l, const ValueRef object, const ValueRef index);
    static ReturnedValue indexedGetterObjectInt(Lookup *l, const ValueRef object, const ValueRef index);

    static void indexedSetterGeneric(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef v);
    static void indexedSetterFallback(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef value);
    static void indexedSetterObjectInt(Lookup *l, const ValueRef object, const ValueRef index, const ValueRef v);

    static ReturnedValue getterGeneric(Lookup *l, const ValueRef object);
    static ReturnedValue getterTwoClasses(Lookup *l, const ValueRef object);
    static ReturnedValue getterFallback(Lookup *l, const ValueRef object);

    static ReturnedValue getter0(Lookup *l, const ValueRef object);
    static ReturnedValue getter1(Lookup *l, const ValueRef object);
    static ReturnedValue getter2(Lookup *l, const ValueRef object);
    static ReturnedValue getter0getter0(Lookup *l, const ValueRef object);
    static ReturnedValue getter0getter1(Lookup *l, const ValueRef object);
    static ReturnedValue getter1getter1(Lookup *l, const ValueRef object);
    static ReturnedValue getterAccessor0(Lookup *l, const ValueRef object);
    static ReturnedValue getterAccessor1(Lookup *l, const ValueRef object);
    static ReturnedValue getterAccessor2(Lookup *l, const ValueRef object);

    static ReturnedValue primitiveGetter0(Lookup *l, const ValueRef object);
    static ReturnedValue primitiveGetter1(Lookup *l, const ValueRef object);
    static ReturnedValue primitiveGetterAccessor0(Lookup *l, const ValueRef object);
    static ReturnedValue primitiveGetterAccessor1(Lookup *l, const ValueRef object);
    static ReturnedValue stringLengthGetter(Lookup *l, const ValueRef object);
    static ReturnedValue arrayLengthGetter(Lookup *l, const ValueRef object);

    static ReturnedValue globalGetterGeneric(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetter0(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetter1(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetter2(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetterAccessor0(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetterAccessor1(Lookup *l, ExecutionContext *ctx);
    static ReturnedValue globalGetterAccessor2(Lookup *l, ExecutionContext *ctx);

    static void setterGeneric(Lookup *l, const ValueRef object, const ValueRef value);
    static void setterTwoClasses(Lookup *l, const ValueRef object, const ValueRef value);
    static void setterFallback(Lookup *l, const ValueRef object, const ValueRef value);
    static void setter0(Lookup *l, const ValueRef object, const ValueRef value);
    static void setterInsert0(Lookup *l, const ValueRef object, const ValueRef value);
    static void setterInsert1(Lookup *l, const ValueRef object, const ValueRef value);
    static void setterInsert2(Lookup *l, const ValueRef object, const ValueRef value);
    static void setter0setter0(Lookup *l, const ValueRef object, const ValueRef value);

    ReturnedValue lookup(ValueRef thisObject, Object *obj, PropertyAttributes *attrs);
    ReturnedValue lookup(Object *obj, PropertyAttributes *attrs);

};

}

QT_END_NAMESPACE

#endif
