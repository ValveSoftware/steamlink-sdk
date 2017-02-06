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
#ifndef QV4VALUE_P_H
#define QV4VALUE_P_H

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

#include <limits.h>

#include <QtCore/QString>
#include "qv4global_p.h"
#include <private/qv4heap_p.h>

#if QT_POINTER_SIZE == 8
#define QV4_USE_64_BIT_VALUE_ENCODING
#endif

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {
    struct Base;
}

typedef uint Bool;

struct Q_QML_PRIVATE_EXPORT Value
{
private:
    /*
        We use two different ways of encoding JS values. One for 32bit and one for 64bit systems.

        In both cases, we use 8 bytes for a value and a different variant of NaN boxing. A Double
        NaN (actually -qNaN) is indicated by a number that has the top 13 bits set, and for a
        signalling NaN it is the top 14 bits. The other values are usually set to 0 by the
        processor, and are thus free for us to store other data. We keep pointers in there for
        managed objects, and encode the other types using the free space given to use by the unused
        bits for NaN values. This also works for pointers on 64 bit systems, as they all currently
        only have 48 bits of addressable memory. (Note: we do leave the lower 49 bits available for
        pointers.)

        On 32bit, we store doubles as doubles. All other values, have the high 32bits set to a value
        that will make the number a NaN. The Masks below are used for encoding the other types.

        On 64 bit, we xor Doubles with (0xffff8000 << 32). That has the effect that no doubles will
        get encoded with bits 63-49 all set to 0. We then use bit 48 to distinguish between
        managed/undefined (0), or Null/Int/Bool/Empty (1). So, storing a 49 bit pointer will leave
        the top 15 bits 0, which is exactly the 'natural' representation of pointers. If bit 49 is
        set, bit 48 indicates Empty (0) or integer-convertible (1). Then the 3 bit below that are
        used to encode Null/Int/Bool.

        On both 32bit and 64bit, Undefined is encoded as a managed pointer with value 0. This is
        the same as a nullptr.

        Specific bit-sequences:
        0 = always 0
        1 = always 1
        x = stored value
        a,b,c,d = specific bit values, see notes

        64bit:

        32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210 |
        66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000 | JS Value
        ------------------------------------------------------------------------+--------------
        00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 | Undefined
        00000000 0000000x xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Managed (heap pointer)
        a0000000 0000bc00 00000000 00000000 00000000 00000000 00000000 00000000 | NaN/Inf
        dddddddd ddddddxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | double
        00000000 00000010 00000000 00000000 00000000 00000000 00000000 00000000 | empty (non-sparse array hole)
        00000000 00000011 10000000 00000000 00000000 00000000 00000000 00000000 | Null
        00000000 00000011 01000000 00000000 00000000 00000000 00000000 0000000x | Bool
        00000000 00000011 00100000 00000000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Int

        Notes:
        - a: xor-ed signbit, always 1 for NaN
        - bc, xor-ed values: 11 = inf, 10 = sNaN, 01 = qNaN, 00 = boxed value
        - d: xor-ed bits, where at least one bit is set, so: (val >> (64-14)) > 0
        - Undefined maps to C++ nullptr, so the "default" initialization is the same for both C++
          and JS
        - Managed has the left 15 bits set to 0, so: (val >> (64-15)) == 0
        - empty, Null, Bool, and Int have the left 14 bits set to 0, and bit 49 set to 1,
          so: (val >> (64-15)) == 1
        - Null, Bool, and Int have bit 48 set, indicating integer-convertible
        - xoring _val with NaNEncodeMask will convert to a double in "natural" representation, where
          any non double results in a NaN

        32bit:

        32109876 54321098 76543210 98765432 10987654 32109876 54321098 76543210 |
        66665555 55555544 44444444 33333333 33222222 22221111 11111100 00000000 | JS Value
        ------------------------------------------------------------------------+--------------
        01111111 11111100 00000000 00000000 00000000 00000000 00000000 00000000 | Undefined
        01111111 11111100 00000000 00000000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Managed (heap pointer)
        a1111111 1111bc00 00000000 00000000 00000000 00000000 00000000 00000000 | NaN/Inf
        xddddddd ddddddxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | double
        01111111 11111110 00000000 00000000 00000000 00000000 00000000 00000000 | empty (non-sparse array hole)
        01111111 11111111 10000000 00000000 00000000 00000000 00000000 00000000 | Null
        01111111 11111111 01000000 00000000 00000000 00000000 00000000 0000000x | Bool
        01111111 11111111 00100000 00000000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | Int

        Notes:
        - the upper 32 bits are the tag, the lower 32 bits the value
        - Undefined has a nullptr in the value, Managed has a non-nullptr stored in the value
        - a: sign bit, always 0 for NaN
        - b,c: 00=inf, 01 = sNaN, 10 = qNaN, 11 = boxed value
        - d: stored double value, as long as not *all* of them are 1, because that's a boxed value
          (see above)
        - empty, Null, Bool, and Int have bit 63 set to 0, bits 62-50 set to 1 (same as undefined
          and managed), and bit 49 set to 1 (where undefined and managed have it set to 0)
        - Null, Bool, and Int have bit 48 set, indicating integer-convertible
    */

    quint64 _val;

public:
    Q_ALWAYS_INLINE quint64 &rawValueRef() { return _val; }
    Q_ALWAYS_INLINE quint64 rawValue() const { return _val; }
    Q_ALWAYS_INLINE void setRawValue(quint64 raw) { _val = raw; }

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    static inline int valueOffset() { return 0; }
    static inline int tagOffset() { return 4; }
#else // !Q_LITTLE_ENDIAN
    static inline int valueOffset() { return 4; }
    static inline int tagOffset() { return 0; }
#endif
    Q_ALWAYS_INLINE void setTagValue(quint32 tag, quint32 value) { _val = quint64(tag) << 32 | value; }
    Q_ALWAYS_INLINE quint32 value() const { return _val & quint64(~quint32(0)); }
    Q_ALWAYS_INLINE quint32 tag() const { return _val >> 32; }

#if defined(QV4_USE_64_BIT_VALUE_ENCODING)
    Q_ALWAYS_INLINE Heap::Base *m() const
    {
        Heap::Base *b;
        memcpy(&b, &_val, 8);
        return b;
    }
    Q_ALWAYS_INLINE void setM(Heap::Base *b)
    {
        memcpy(&_val, &b, 8);
    }
#else // !QV4_USE_64_BIT_VALUE_ENCODING
    Q_ALWAYS_INLINE Heap::Base *m() const
    {
        Q_STATIC_ASSERT(sizeof(Heap::Base*) == sizeof(quint32));
        Heap::Base *b;
        quint32 v = value();
        memcpy(&b, &v, 4);
        return b;
    }
    Q_ALWAYS_INLINE void setM(Heap::Base *b)
    {
        quint32 v;
        memcpy(&v, &b, 4);
        setTagValue(Managed_Type_Internal, v);
    }
#endif

    Q_ALWAYS_INLINE int int_32() const
    {
        return int(value());
    }
    Q_ALWAYS_INLINE void setInt_32(int i)
    {
        setTagValue(Integer_Type_Internal, quint32(i));
    }
    Q_ALWAYS_INLINE uint uint_32() const { return value(); }

    Q_ALWAYS_INLINE void setEmpty()
    {
        setTagValue(Empty_Type_Internal, value());
    }

    Q_ALWAYS_INLINE void setEmpty(int i)
    {
        setTagValue(Empty_Type_Internal, quint32(i));
    }

    Q_ALWAYS_INLINE void setEmpty(quint32 i)
    {
        setTagValue(Empty_Type_Internal, i);
    }

    Q_ALWAYS_INLINE quint32 emptyValue()
    {
        Q_ASSERT(isEmpty());
        return quint32(value());
    }

    enum Type {
        Undefined_Type,
        Managed_Type,
        Empty_Type,
        Integer_Type,
        Boolean_Type,
        Null_Type,
        Double_Type
    };

    inline Type type() const {
        if (isUndefined()) return Undefined_Type;
        if (isManaged()) return Managed_Type;
        if (isEmpty()) return Empty_Type;
        if (isInteger()) return Integer_Type;
        if (isBoolean()) return Boolean_Type;
        if (isNull()) return Null_Type;
        Q_ASSERT(isDouble()); return Double_Type;
    }

#ifndef QV4_USE_64_BIT_VALUE_ENCODING
    enum Masks {
        SilentNaNBit           =                  0x00040000,
        NaN_Mask               =                  0x7ff80000,
        NotDouble_Mask         =                  0x7ffa0000,
        Immediate_Mask         = NotDouble_Mask | 0x00020000u | SilentNaNBit,
        Tag_Shift = 32
    };

    enum {
        Managed_Type_Internal  = NotDouble_Mask
    };
#else
    static const quint64 NaNEncodeMask  = 0xfffc000000000000ll;
    static const quint64 Immediate_Mask = 0x00020000u; // bit 49

    enum Masks {
        NaN_Mask = 0x7ff80000,
    };
    enum {
        IsDouble_Shift = 64-14,
        IsManagedOrUndefined_Shift = 64-15,
        IsIntegerConvertible_Shift = 64-16,
        Tag_Shift = 32,
        IsDoubleTag_Shift = IsDouble_Shift - Tag_Shift,
        Managed_Type_Internal = 0
    };
#endif
    enum ValueTypeInternal {
        Empty_Type_Internal   = Immediate_Mask   | 0,
        ConvertibleToInt      = Immediate_Mask   | 0x10000u, // bit 48
        Null_Type_Internal    = ConvertibleToInt | 0x08000u,
        Boolean_Type_Internal = ConvertibleToInt | 0x04000u,
        Integer_Type_Internal = ConvertibleToInt | 0x02000u
    };

    // used internally in property
    inline bool isEmpty() const { return tag() == Empty_Type_Internal; }
    inline bool isNull() const { return tag() == Null_Type_Internal; }
    inline bool isBoolean() const { return tag() == Boolean_Type_Internal; }
    inline bool isInteger() const { return tag() == Integer_Type_Internal; }
    inline bool isNullOrUndefined() const { return isNull() || isUndefined(); }
    inline bool isNumber() const { return isDouble() || isInteger(); }

#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    inline bool isUndefined() const { return _val == 0; }
    inline bool isDouble() const { return (_val >> IsDouble_Shift); }
    inline bool isManaged() const { return !isUndefined() && ((_val >> IsManagedOrUndefined_Shift) == 0); }

    inline bool integerCompatible() const {
        return (_val >> IsIntegerConvertible_Shift) == 3;
    }
    static inline bool integerCompatible(Value a, Value b) {
        return a.integerCompatible() && b.integerCompatible();
    }
    static inline bool bothDouble(Value a, Value b) {
        return a.isDouble() && b.isDouble();
    }
    inline bool isNaN() const { return (tag() & 0x7ffc0000  ) == 0x00040000; }
#else
    inline bool isUndefined() const { return tag() == Managed_Type_Internal && value() == 0; }
    inline bool isDouble() const { return (tag() & NotDouble_Mask) != NotDouble_Mask; }
    inline bool isManaged() const { return tag() == Managed_Type_Internal && !isUndefined(); }
    inline bool integerCompatible() const { return (tag() & ConvertibleToInt) == ConvertibleToInt; }
    static inline bool integerCompatible(Value a, Value b) {
        return ((a.tag() & b.tag()) & ConvertibleToInt) == ConvertibleToInt;
    }
    static inline bool bothDouble(Value a, Value b) {
        return ((a.tag() | b.tag()) & NotDouble_Mask) != NotDouble_Mask;
    }
    inline bool isNaN() const { return (tag() & QV4::Value::NotDouble_Mask) == QV4::Value::NaN_Mask; }
#endif
    Q_ALWAYS_INLINE double doubleValue() const {
        Q_ASSERT(isDouble());
        double d;
        quint64 v = _val;
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
        v ^= NaNEncodeMask;
#endif
        memcpy(&d, &v, 8);
        return d;
    }
    Q_ALWAYS_INLINE void setDouble(double d) {
        memcpy(&_val, &d, 8);
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
        _val ^= NaNEncodeMask;
#endif
        Q_ASSERT(isDouble());
    }
    inline bool isString() const;
    inline bool isObject() const;
    inline bool isInt32() {
        if (tag() == Integer_Type_Internal)
            return true;
        if (isDouble()) {
            double d = doubleValue();
            int i = (int)d;
            if (i == d) {
                setInt_32(i);
                return true;
            }
        }
        return false;
    }
    double asDouble() const {
        if (tag() == Integer_Type_Internal)
            return int_32();
        return doubleValue();
    }

    bool booleanValue() const {
        return int_32();
    }
    int integerValue() const {
        return int_32();
    }

    Q_ALWAYS_INLINE String *stringValue() const {
        if (!isString())
            return nullptr;
        return m() ? reinterpret_cast<String*>(const_cast<Value *>(this)) : 0;
    }
    Q_ALWAYS_INLINE Object *objectValue() const {
        if (!isObject())
            return nullptr;
        return m() ? reinterpret_cast<Object*>(const_cast<Value *>(this)) : 0;
    }
    Q_ALWAYS_INLINE Managed *managed() const {
        if (!isManaged())
            return nullptr;
        return m() ? reinterpret_cast<Managed*>(const_cast<Value *>(this)) : 0;
    }
    Q_ALWAYS_INLINE Heap::Base *heapObject() const {
        return isManaged() ? m() : nullptr;
    }

    static inline Value fromHeapObject(Heap::Base *m)
    {
        Value v;
        v.setM(m);
        return v;
    }

    int toUInt16() const;
    inline int toInt32() const;
    inline unsigned int toUInt32() const;

    bool toBoolean() const;
    double toInteger() const;
    inline double toNumber() const;
    double toNumberImpl() const;
    QString toQStringNoThrow() const;
    QString toQString() const;
    Heap::String *toString(ExecutionEngine *e) const;
    Heap::Object *toObject(ExecutionEngine *e) const;

    inline bool isPrimitive() const;
    inline bool tryIntegerConversion() {
        bool b = integerCompatible();
        if (b)
            setTagValue(Integer_Type_Internal, value());
        return b;
    }

    template <typename T>
    const T *as() const {
        if (!m() || !isManaged())
            return 0;

        Q_ASSERT(m()->vtable());
#if !defined(QT_NO_QOBJECT_CHECK)
        static_cast<const T *>(this)->qt_check_for_QMANAGED_macro(static_cast<const T *>(this));
#endif
        const VTable *vt = m()->vtable();
        while (vt) {
            if (vt == T::staticVTable())
                return static_cast<const T *>(this);
            vt = vt->parent;
        }
        return 0;
    }
    template <typename T>
    T *as() {
        if (isManaged())
            return const_cast<T *>(const_cast<const Value *>(this)->as<T>());
        else
            return nullptr;
    }

    template<typename T> inline T *cast() {
        return static_cast<T *>(managed());
    }
    template<typename T> inline const T *cast() const {
        return static_cast<const T *>(managed());
    }

    inline uint asArrayIndex() const;
#ifndef V4_BOOTSTRAP
    uint asArrayLength(bool *ok) const;
#endif

    ReturnedValue asReturnedValue() const { return _val; }
    static Value fromReturnedValue(ReturnedValue val) { Value v; v._val = val; return v; }

    // Section 9.12
    bool sameValue(Value other) const;

    inline void mark(ExecutionEngine *e);

    Value &operator =(const ScopedValue &v);
    Value &operator=(ReturnedValue v) { _val = v; return *this; }
    Value &operator=(Managed *m) {
        if (!m) {
            setM(0);
        } else {
            _val = reinterpret_cast<Value *>(m)->_val;
        }
        return *this;
    }
    Value &operator=(Heap::Base *o) {
        setM(o);
        return *this;
    }

    template<typename T>
    Value &operator=(const Scoped<T> &t);
};
V4_ASSERT_IS_TRIVIAL(Value)

inline bool Value::isString() const
{
    if (!isManaged())
        return false;
    return m() && m()->vtable()->isString;
}
inline bool Value::isObject() const
{
    if (!isManaged())
        return false;
    return m() && m()->vtable()->isObject;
}

inline bool Value::isPrimitive() const
{
    return !isObject();
}

inline double Value::toNumber() const
{
    if (isInteger())
        return int_32();
    if (isDouble())
        return doubleValue();
    return toNumberImpl();
}


#ifndef V4_BOOTSTRAP
inline uint Value::asArrayIndex() const
{
#ifdef QV4_USE_64_BIT_VALUE_ENCODING
    if (!isNumber())
        return UINT_MAX;
    if (isInteger())
        return int_32() >= 0 ? (uint)int_32() : UINT_MAX;
#else
    if (isInteger() && int_32() >= 0)
        return (uint)int_32();
    if (!isDouble())
        return UINT_MAX;
#endif
    double d = doubleValue();
    uint idx = (uint)d;
    if (idx != d)
        return UINT_MAX;
    return idx;
}
#endif

inline
ReturnedValue Heap::Base::asReturnedValue() const
{
    return Value::fromHeapObject(const_cast<Heap::Base *>(this)).asReturnedValue();
}



struct Q_QML_PRIVATE_EXPORT Primitive : public Value
{
    inline static Primitive emptyValue();
    inline static Primitive emptyValue(uint v);
    static inline Primitive fromBoolean(bool b);
    static inline Primitive fromInt32(int i);
    inline static Primitive undefinedValue();
    static inline Primitive nullValue();
    static inline Primitive fromDouble(double d);
    static inline Primitive fromUInt32(uint i);

    using Value::toInt32;
    using Value::toUInt32;

    static double toInteger(double fromNumber);
    static int toInt32(double value);
    static unsigned int toUInt32(double value);
};

inline Primitive Primitive::undefinedValue()
{
    Primitive v;
    v.setM(Q_NULLPTR);
    return v;
}

inline Primitive Primitive::emptyValue()
{
    Primitive v;
    v.setEmpty(0);
    return v;
}

inline Primitive Primitive::emptyValue(uint e)
{
    Primitive v;
    v.setEmpty(e);
    return v;
}

inline Primitive Primitive::nullValue()
{
    Primitive v;
    v.setTagValue(Null_Type_Internal, 0);
    return v;
}

inline Primitive Primitive::fromBoolean(bool b)
{
    Primitive v;
    v.setTagValue(Boolean_Type_Internal, b);
    return v;
}

inline Primitive Primitive::fromDouble(double d)
{
    Primitive v;
    v.setDouble(d);
    return v;
}

inline Primitive Primitive::fromInt32(int i)
{
    Primitive v;
    v.setInt_32(i);
    return v;
}

inline Primitive Primitive::fromUInt32(uint i)
{
    Primitive v;
    if (i < INT_MAX) {
        v.setTagValue(Integer_Type_Internal, i);
    } else {
        v.setDouble(i);
    }
    return v;
}

struct Encode {
    static ReturnedValue undefined() {
        return Primitive::undefinedValue().rawValue();
    }
    static ReturnedValue null() {
        return Primitive::nullValue().rawValue();
    }

    Encode(bool b) {
        val = Primitive::fromBoolean(b).rawValue();
    }
    Encode(double d) {
        val = Primitive::fromDouble(d).rawValue();
    }
    Encode(int i) {
        val = Primitive::fromInt32(i).rawValue();
    }
    Encode(uint i) {
        val = Primitive::fromUInt32(i).rawValue();
    }
    Encode(ReturnedValue v) {
        val = v;
    }

    Encode(Heap::Base *o) {
        Q_ASSERT(o);
        val = Value::fromHeapObject(o).asReturnedValue();
    }

    operator ReturnedValue() const {
        return val;
    }
    quint64 val;
private:
    Encode(void *);
};

template<typename T>
ReturnedValue value_convert(ExecutionEngine *e, const Value &v);

inline int Value::toInt32() const
{
    if (isInteger())
        return int_32();
    double d = isNumber() ? doubleValue() : toNumberImpl();

    const double D32 = 4294967296.0;
    const double D31 = D32 / 2.0;

    if ((d >= -D31 && d < D31))
        return static_cast<int>(d);

    return Primitive::toInt32(d);
}

inline unsigned int Value::toUInt32() const
{
    return (unsigned int)toInt32();
}


}

QT_END_NAMESPACE

#endif // QV4VALUE_DEF_P_H
