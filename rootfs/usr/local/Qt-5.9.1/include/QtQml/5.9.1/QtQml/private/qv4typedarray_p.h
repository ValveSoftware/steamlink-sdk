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
#ifndef QV4TYPEDARRAY_H
#define QV4TYPEDARRAY_H

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

#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4arraybuffer_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ArrayBuffer;

typedef ReturnedValue (*TypedArrayRead)(const char *data, int index);
typedef void (*TypedArrayWrite)(ExecutionEngine *engine, char *data, int index, const Value &value);

struct TypedArrayOperations {
    int bytesPerElement;
    const char *name;
    TypedArrayRead read;
    TypedArrayWrite write;
};

namespace Heap {

struct TypedArray : Object {
    enum Type {
        Int8Array,
        UInt8Array,
        UInt8ClampedArray,
        Int16Array,
        UInt16Array,
        Int32Array,
        UInt32Array,
        Float32Array,
        Float64Array,
        NTypes
    };

    void init(Type t);

    const TypedArrayOperations *type;
    Pointer<ArrayBuffer> buffer;
    uint byteLength;
    uint byteOffset;
    Type arrayType;
};

struct TypedArrayCtor : FunctionObject {
    void init(QV4::ExecutionContext *scope, TypedArray::Type t);

    TypedArray::Type type;
};

struct TypedArrayPrototype : Object {
    inline void init(TypedArray::Type t);
    TypedArray::Type type;
};


}

struct Q_QML_PRIVATE_EXPORT TypedArray : Object
{
    V4_OBJECT2(TypedArray, Object)

    static Heap::TypedArray *create(QV4::ExecutionEngine *e, Heap::TypedArray::Type t);

    uint byteLength() const {
        return d()->byteLength;
    }

    uint length() const {
        return d()->byteLength/d()->type->bytesPerElement;
    }

    QTypedArrayData<char> *arrayData() {
        return d()->buffer->data;
    }

    Heap::TypedArray::Type arrayType() const {
        return d()->arrayType;
    }

    static void markObjects(Heap::Base *that, ExecutionEngine *e);
    static ReturnedValue getIndexed(const Managed *m, uint index, bool *hasProperty);
    static void putIndexed(Managed *m, uint index, const Value &value);
};

struct TypedArrayCtor: FunctionObject
{
    V4_OBJECT2(TypedArrayCtor, FunctionObject)

    static void construct(const Managed *m, Scope &scope, CallData *callData);
    static void call(const Managed *that, Scope &scope, CallData *callData);
};


struct TypedArrayPrototype : Object
{
    V4_OBJECT2(TypedArrayPrototype, Object)
    V4_PROTOTYPE(objectPrototype)

    void init(ExecutionEngine *engine, TypedArrayCtor *ctor);

    static void method_get_buffer(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_byteLength(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_byteOffset(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_length(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_set(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_subarray(const BuiltinFunction *, Scope &scope, CallData *callData);
};

inline void
Heap::TypedArrayPrototype::init(TypedArray::Type t)
{
    Object::init();
    type = t;
}

} // namespace QV4

QT_END_NAMESPACE

#endif
