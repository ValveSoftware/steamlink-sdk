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

#include <QtCore/qstring.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qdatetime.h>
#include "qjsengine.h"
#include "qjsvalue.h"
#include "qjsvalue_p.h"
#include "qv4value_p.h"
#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4dateobject_p.h"
#include "qv4runtime_p.h"
#include "qv4variantobject_p.h"
#include "qv4regexpobject_p.h"
#include "qv4errorobject_p.h"
#include "private/qv8engine_p.h"
#include <private/qv4mm_p.h>
#include <private/qv4scopedvalue_p.h>
#include <private/qv4qobjectwrapper_p.h>

/*!
  \since 5.0
  \class QJSValue

  \brief The QJSValue class acts as a container for Qt/JavaScript data types.

  \ingroup qtjavascript
  \inmodule QtQml

  QJSValue supports the types defined in the \l{ECMA-262}
  standard: The primitive types, which are Undefined, Null, Boolean,
  Number, and String; and the Object type. Additionally, built-in
  support is provided for Qt/C++ types such as QVariant and QObject.

  For the object-based types (including Date and RegExp), use the
  newT() functions in QJSEngine (e.g. QJSEngine::newObject())
  to create a QJSValue of the desired type. For the primitive types,
  use one of the QJSValue constructor overloads.

  The methods named isT() (e.g. isBool(), isUndefined()) can be
  used to test if a value is of a certain type. The methods named
  toT() (e.g. toBool(), toString()) can be used to convert a
  QJSValue to another type. You can also use the generic
  QJSValue_cast() function.

  Object values have zero or more properties which are themselves
  QJSValues. Use setProperty() to set a property of an object, and
  call property() to retrieve the value of a property.

  \snippet code/src_script_qjsvalue.cpp 0

  If you want to iterate over the properties of a script object, use
  the QJSValueIterator class.

  Object values have an internal \c{prototype} property, which can be
  accessed with prototype() and setPrototype().

  Function objects (objects for which isCallable()) returns true) can
  be invoked by calling call(). Constructor functions can be used to
  construct new objects by calling callAsConstructor().

  Use equals() or strictlyEquals() to compare a QJSValue to another.

  Note that a QJSValue for which isObject() is true only carries a
  reference to an actual object; copying the QJSValue will only
  copy the object reference, not the object itself. If you want to
  clone an object (i.e. copy an object's properties to another
  object), you can do so with the help of a \c{for-in} statement in
  script code, or QJSValueIterator in C++.

  \sa QJSEngine, QJSValueIterator
*/

/*!
    \enum QJSValue::SpecialValue

    This enum is used to specify a single-valued type.

    \value UndefinedValue An undefined value.

    \value NullValue A null value.
*/

QT_BEGIN_NAMESPACE

using namespace QV4;

/*!
  Constructs a new QJSValue with a boolean \a value.
*/
QJSValue::QJSValue(bool value)
{
    QJSValuePrivate::setVariant(this, QVariant(value));
}

/*!
  \internal
*/
QJSValue::QJSValue(ExecutionEngine *e, quint64 val)
{
    QJSValuePrivate::setValue(this, e, val);
}

/*!
  Constructs a new QJSValue with a number \a value.
*/
QJSValue::QJSValue(int value)
{
    QJSValuePrivate::setVariant(this, QVariant(value));
}

/*!
  Constructs a new QJSValue with a number \a value.
*/
QJSValue::QJSValue(uint value)
{
    QJSValuePrivate::setVariant(this, QVariant((double)value));
}

/*!
  Constructs a new QJSValue with a number \a value.
*/
QJSValue::QJSValue(double value)
{
    QJSValuePrivate::setVariant(this, QVariant(value));
}

/*!
  Constructs a new QJSValue with a string \a value.
*/
QJSValue::QJSValue(const QString& value)
{
    QJSValuePrivate::setVariant(this, QVariant(value));
}

/*!
  Constructs a new QJSValue with a special \a value.
*/
QJSValue::QJSValue(SpecialValue value)
    : d(0)
{
    if (value == NullValue)
        QJSValuePrivate::setVariant(this, QVariant::fromValue(nullptr));
}

/*!
  Constructs a new QJSValue with a string \a value.
*/
QJSValue::QJSValue(const QLatin1String &value)
{
    QJSValuePrivate::setVariant(this, QVariant(value));
}

/*!
  Constructs a new QJSValue with a string \a value.
*/
#ifndef QT_NO_CAST_FROM_ASCII
QJSValue::QJSValue(const char *value)
{
    QJSValuePrivate::setVariant(this, QVariant(QString::fromUtf8(value)));
}
#endif

/*!
  Constructs a new QJSValue that is a copy of \a other.

  Note that if \a other is an object (i.e., isObject() would return
  true), then only a reference to the underlying object is copied into
  the new script value (i.e., the object itself is not copied).
*/
QJSValue::QJSValue(const QJSValue& other)
    : d(0)
{
    QV4::Value *v = QJSValuePrivate::getValue(&other);
    if (v) {
        QJSValuePrivate::setValue(this, QJSValuePrivate::engine(&other), *v);
    } else if (QVariant *v = QJSValuePrivate::getVariant(&other)) {
        QJSValuePrivate::setVariant(this, *v);
    }
}

/*!
    \fn QJSValue::QJSValue(QJSValue && other)

    Move constructor. Moves from \a other into this QJSValue object.
*/

/*!
    \fn QJSValue &operator=(QJSValue && other)

    Move-assigns \a other to this QJSValue object.
*/

/*!
    Destroys this QJSValue.
*/
QJSValue::~QJSValue()
{
    QJSValuePrivate::free(this);
}

/*!
  Returns true if this QJSValue is of the primitive type Boolean;
  otherwise returns false.

  \sa toBool()
*/
bool QJSValue::isBool() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val)
        return val->isBoolean();
    QVariant *variant = QJSValuePrivate::getVariant(this);
    return variant && variant->type() == QVariant::Bool;
}

/*!
  Returns true if this QJSValue is of the primitive type Number;
  otherwise returns false.

  \sa toNumber()
*/
bool QJSValue::isNumber() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val)
        return val->isNumber();
    QVariant *variant = QJSValuePrivate::getVariant(this);
    if (!variant)
        return false;

    switch (variant->userType()) {
    case QMetaType::Double:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

/*!
  Returns true if this QJSValue is of the primitive type Null;
  otherwise returns false.
*/
bool QJSValue::isNull() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val)
        return val->isNull();
    QVariant *variant = QJSValuePrivate::getVariant(this);
    if (!variant)
        return false;
    const int type = variant->userType();
    return type == QMetaType::Nullptr || type == QMetaType::VoidStar;
}

/*!
  Returns true if this QJSValue is of the primitive type String;
  otherwise returns false.

  \sa toString()
*/
bool QJSValue::isString() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val)
        return val->isString();
    QVariant *variant = QJSValuePrivate::getVariant(this);
    return variant && variant->userType() == QMetaType::QString;
}

/*!
  Returns true if this QJSValue is of the primitive type Undefined;
  otherwise returns false.
*/
bool QJSValue::isUndefined() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val)
        return val->isUndefined();
    QVariant *variant = QJSValuePrivate::getVariant(this);
    return !variant || variant->userType() == QMetaType::UnknownType || variant->userType() == QMetaType::Void;
}

/*!
  Returns true if this QJSValue is an object of the Error class;
  otherwise returns false.

  \sa {QJSEngine#Script Exceptions}{QJSEngine - Script Exceptions}
*/
bool QJSValue::isError() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return false;
    return val->as<ErrorObject>();
}

/*!
  Returns true if this QJSValue is an object of the Array class;
  otherwise returns false.

  \sa QJSEngine::newArray()
*/
bool QJSValue::isArray() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return false;
    return val->as<ArrayObject>();
}

/*!
  Returns true if this QJSValue is of the Object type; otherwise
  returns false.

  Note that function values, variant values, and QObject values are
  objects, so this function returns true for such values.

  \sa QJSEngine::newObject()
*/
bool QJSValue::isObject() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return false;
    return val->as<Object>();
}

/*!
  Returns true if this QJSValue can be called a function, otherwise
  returns false.

  \sa call()
*/
bool QJSValue::isCallable() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return false;
    return val->as<FunctionObject>();
}

/*!
  Returns true if this QJSValue is a variant value;
  otherwise returns false.

  \sa toVariant()
*/
bool QJSValue::isVariant() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return false;
    return val->as<QV4::VariantObject>();
}

/*!
  Returns the string value of this QJSValue, as defined in
  \l{ECMA-262} section 9.8, "ToString".

  Note that if this QJSValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's toString() function (and possibly valueOf()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isString()
*/
QString QJSValue::toString() const
{
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);

    if (!val) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        Q_ASSERT(variant);
        if (variant->type() == QVariant::Map)
            return QStringLiteral("[object Object]");
        else if (variant->type() == QVariant::List) {
            const QVariantList list = variant->toList();
            QString result;
            for (int i = 0; i < list.count(); ++i) {
                if (i > 0)
                    result.append(QLatin1Char(','));
                result.append(list.at(i).toString());
            }
            return result;
        }
        return variant->toString();
    }
    return val->toQStringNoThrow();
}

/*!
  Returns the number value of this QJSValue, as defined in
  \l{ECMA-262} section 9.3, "ToNumber".

  Note that if this QJSValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isNumber(), toInt(), toUInt()
*/
double QJSValue::toNumber() const
{
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);

    if (!val) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        Q_ASSERT(variant);

        if (variant->type() == QVariant::String)
            return RuntimeHelpers::stringToNumber(variant->toString());
        else if (variant->canConvert<double>())
            return variant->value<double>();
        else
            return std::numeric_limits<double>::quiet_NaN();
    }

    double dbl = val->toNumber();
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (engine && engine->hasException) {
        engine->catchException();
        return 0;
    }
    return dbl;
}

/*!
  Returns the boolean value of this QJSValue, using the conversion
  rules described in \l{ECMA-262} section 9.2, "ToBoolean".

  Note that if this QJSValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isBool()
*/
bool QJSValue::toBool() const
{
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);

    if (!val) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        if (variant->userType() == QMetaType::QString)
            return variant->toString().length() > 0;
        else
            return variant->toBool();
    }

    bool b = val->toBoolean();
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (engine && engine->hasException) {
        engine->catchException();
        return false;
    }
    return b;
}

/*!
  Returns the signed 32-bit integer value of this QJSValue, using
  the conversion rules described in \l{ECMA-262} section 9.5, "ToInt32".

  Note that if this QJSValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber(), toUInt()
*/
qint32 QJSValue::toInt() const
{
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);

    if (!val) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        if (variant->userType() == QMetaType::QString)
            return QV4::Primitive::toInt32(RuntimeHelpers::stringToNumber(variant->toString()));
        else
            return variant->toInt();
    }

    qint32 i = val->toInt32();
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (engine && engine->hasException) {
        engine->catchException();
        return 0;
    }
    return i;
}

/*!
  Returns the unsigned 32-bit integer value of this QJSValue, using
  the conversion rules described in \l{ECMA-262} section 9.6, "ToUint32".

  Note that if this QJSValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber(), toInt()
*/
quint32 QJSValue::toUInt() const
{
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);

    if (!val) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        if (variant->userType() == QMetaType::QString)
            return QV4::Primitive::toUInt32(RuntimeHelpers::stringToNumber(variant->toString()));
        else
            return variant->toUInt();
    }

    quint32 u = val->toUInt32();
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (engine && engine->hasException) {
        engine->catchException();
        return 0;
    }
    return u;
}

/*!
  Returns the QVariant value of this QJSValue, if it can be
  converted to a QVariant; otherwise returns an invalid QVariant.
  The conversion is performed according to the following table:

    \table
    \header \li Input Type \li Result
    \row    \li Undefined  \li An invalid QVariant.
    \row    \li Null       \li A QVariant containing a null pointer (QMetaType::Nullptr).
    \row    \li Boolean    \li A QVariant containing the value of the boolean.
    \row    \li Number     \li A QVariant containing the value of the number.
    \row    \li String     \li A QVariant containing the value of the string.
    \row    \li QVariant Object \li The result is the QVariant value of the object (no conversion).
    \row    \li QObject Object \li A QVariant containing a pointer to the QObject.
    \row    \li Date Object \li A QVariant containing the date value (toDateTime()).
    \row    \li RegExp Object \li A QVariant containing the regular expression value.
    \row    \li Array Object \li The array is converted to a QVariantList. Each element is converted to a QVariant, recursively; cyclic references are not followed.
    \row    \li Object     \li The object is converted to a QVariantMap. Each property is converted to a QVariant, recursively; cyclic references are not followed.
    \endtable

  \sa isVariant()
*/
QVariant QJSValue::toVariant() const
{
    QVariant *variant = QJSValuePrivate::getVariant(this);
    if (variant)
        return *variant;

    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(this, &scratch);
    Q_ASSERT(val);

    if (Object *o = val->as<Object>())
        return o->engine()->toVariant(*val, /*typeHint*/ -1, /*createJSValueForObjects*/ false);

    if (val->isString())
        return QVariant(val->stringValue()->toQString());
    if (val->isBoolean())
        return QVariant(val->booleanValue());
    if (val->isNumber()) {
        if (val->isInt32())
            return QVariant(val->integerValue());
        return QVariant(val->asDouble());
    }
    if (val->isNull())
        return QVariant(QMetaType::Nullptr, 0);
    Q_ASSERT(val->isUndefined());
    return QVariant();
}

/*!
  Calls this QJSValue as a function, passing \a args as arguments
  to the function, and using the globalObject() as the "this"-object.
  Returns the value returned from the function.

  If this QJSValue is not callable, call() does nothing and
  returns an undefined QJSValue.

  Calling call() can cause an exception to occur in the script engine;
  in that case, call() returns the value that was thrown (typically an
  \c{Error} object). You can call isError() on the return value to
  determine whether an exception occurred.

  \sa isCallable(), callWithInstance(), callAsConstructor()
*/
QJSValue QJSValue::call(const QJSValueList &args)
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return QJSValue();

    FunctionObject *f = val->as<FunctionObject>();
    if (!f)
        return QJSValue();

    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    Q_ASSERT(engine);

    Scope scope(engine);
    ScopedCallData callData(scope, args.length());
    callData->thisObject = engine->globalObject;
    for (int i = 0; i < args.size(); ++i) {
        if (!QJSValuePrivate::checkEngine(engine, args.at(i))) {
            qWarning("QJSValue::call() failed: cannot call function with argument created in a different engine");
            return QJSValue();
        }
        callData->args[i] = QJSValuePrivate::convertedToValue(engine, args.at(i));
    }

    f->call(scope, callData);
    if (engine->hasException)
        scope.result = engine->catchException();

    return QJSValue(engine, scope.result.asReturnedValue());
}

/*!
  Calls this QJSValue as a function, using \a instance as
  the `this' object in the function call, and passing \a args
  as arguments to the function. Returns the value returned from
  the function.

  If this QJSValue is not a function, call() does nothing
  and returns an undefined QJSValue.

  Note that if \a instance is not an object, the global object
  (see \l{QJSEngine::globalObject()}) will be used as the
  `this' object.

  Calling call() can cause an exception to occur in the script engine;
  in that case, call() returns the value that was thrown (typically an
  \c{Error} object). You can call isError() on the return value to
  determine whether an exception occurred.

  \sa call()
*/
QJSValue QJSValue::callWithInstance(const QJSValue &instance, const QJSValueList &args)
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return QJSValue();

    FunctionObject *f = val->as<FunctionObject>();
    if (!f)
        return QJSValue();

    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    Q_ASSERT(engine);
    Scope scope(engine);

    if (!QJSValuePrivate::checkEngine(engine, instance)) {
        qWarning("QJSValue::call() failed: cannot call function with thisObject created in a different engine");
        return QJSValue();
    }

    ScopedCallData callData(scope, args.size());
    callData->thisObject = QJSValuePrivate::convertedToValue(engine, instance);
    for (int i = 0; i < args.size(); ++i) {
        if (!QJSValuePrivate::checkEngine(engine, args.at(i))) {
            qWarning("QJSValue::call() failed: cannot call function with argument created in a different engine");
            return QJSValue();
        }
        callData->args[i] = QJSValuePrivate::convertedToValue(engine, args.at(i));
    }

    f->call(scope, callData);
    if (engine->hasException)
        scope.result = engine->catchException();

    return QJSValue(engine, scope.result.asReturnedValue());
}

/*!
  Creates a new \c{Object} and calls this QJSValue as a
  constructor, using the created object as the `this' object and
  passing \a args as arguments. If the return value from the
  constructor call is an object, then that object is returned;
  otherwise the default constructed object is returned.

  If this QJSValue is not a function, callAsConstructor() does
  nothing and returns an undefined QJSValue.

  Calling this function can cause an exception to occur in the
  script engine; in that case, the value that was thrown
  (typically an \c{Error} object) is returned. You can call
  isError() on the return value to determine whether an
  exception occurred.

  \sa call(), QJSEngine::newObject()
*/
QJSValue QJSValue::callAsConstructor(const QJSValueList &args)
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (!val)
        return QJSValue();

    FunctionObject *f = val->as<FunctionObject>();
    if (!f)
        return QJSValue();

    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    Q_ASSERT(engine);

    Scope scope(engine);
    ScopedCallData callData(scope, args.size());
    for (int i = 0; i < args.size(); ++i) {
        if (!QJSValuePrivate::checkEngine(engine, args.at(i))) {
            qWarning("QJSValue::callAsConstructor() failed: cannot construct function with argument created in a different engine");
            return QJSValue();
        }
        callData->args[i] = QJSValuePrivate::convertedToValue(engine, args.at(i));
    }

    f->construct(scope, callData);
    if (engine->hasException)
        scope.result = engine->catchException();

    return QJSValue(engine, scope.result.asReturnedValue());
}

#ifdef QT_DEPRECATED

/*!
  \obsolete

  Returns the QJSEngine that created this QJSValue,
  or 0 if this QJSValue is invalid or the value is not
  associated with a particular engine.
*/
QJSEngine* QJSValue::engine() const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (engine)
        return engine->jsEngine();
    return 0;
}

#endif // QT_DEPRECATED

/*!
  If this QJSValue is an object, returns the internal prototype
  (\c{__proto__} property) of this object; otherwise returns an
  undefined QJSValue.

  \sa setPrototype(), isObject()
*/
QJSValue QJSValue::prototype() const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return QJSValue();
    QV4::Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this)->as<Object>());
    if (!o)
        return QJSValue();
    ScopedObject p(scope, o->prototype());
    if (!p)
        return QJSValue(NullValue);
    return QJSValue(o->internalClass()->engine, p.asReturnedValue());
}

/*!
  If this QJSValue is an object, sets the internal prototype
  (\c{__proto__} property) of this object to be \a prototype;
  if the QJSValue is null, it sets the prototype to null;
  otherwise does nothing.

  The internal prototype should not be confused with the public
  property with name "prototype"; the public prototype is usually
  only set on functions that act as constructors.

  \sa prototype(), isObject()
*/
void QJSValue::setPrototype(const QJSValue& prototype)
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return;
    Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return;
    QV4::Value scratch;
    QV4::Value *val = QJSValuePrivate::valueForData(&prototype, &scratch);
    if (!val)
        return;
    if (val->isNull()) {
        o->setPrototype(0);
        return;
    }

    ScopedObject p(scope, val);
    if (!p)
        return;
    if (o->engine() != p->engine()) {
        qWarning("QJSValue::setPrototype() failed: cannot set a prototype created in a different engine");
        return;
    }
    if (!o->setPrototype(p))
        qWarning("QJSValue::setPrototype() failed: cyclic prototype value");
}

/*!
  Assigns the \a other value to this QJSValue.

  Note that if \a other is an object (isObject() returns true),
  only a reference to the underlying object will be assigned;
  the object itself will not be copied.
*/
QJSValue& QJSValue::operator=(const QJSValue& other)
{
    if (d == other.d)
        return *this;

    QJSValuePrivate::free(this);
    d = 0;

    QV4::Value *v = QJSValuePrivate::getValue(&other);
    if (v) {
        QJSValuePrivate::setValue(this, QJSValuePrivate::engine(&other), *v);
    } else if (QVariant *v = QJSValuePrivate::getVariant(&other)) {
        QJSValuePrivate::setVariant(this, *v);
    }
    return *this;
}

static bool js_equal(const QString &string, const QV4::Value &value)
{
    if (value.isString())
        return string == value.stringValue()->toQString();
    if (value.isNumber())
        return RuntimeHelpers::stringToNumber(string) == value.asDouble();
    if (value.isBoolean())
        return RuntimeHelpers::stringToNumber(string) == double(value.booleanValue());
    if (value.isObject()) {
        Scope scope(value.objectValue()->engine());
        ScopedValue p(scope, RuntimeHelpers::toPrimitive(value, PREFERREDTYPE_HINT));
        return js_equal(string, p);
    }
    return false;
}

/*!
  Returns true if this QJSValue is equal to \a other, otherwise
  returns false. The comparison follows the behavior described in
  \l{ECMA-262} section 11.9.3, "The Abstract Equality Comparison
  Algorithm".

  This function can return true even if the type of this QJSValue
  is different from the type of the \a other value; i.e. the
  comparison is not strict.  For example, comparing the number 9 to
  the string "9" returns true; comparing an undefined value to a null
  value returns true; comparing a \c{Number} object whose primitive
  value is 6 to a \c{String} object whose primitive value is "6"
  returns true; and comparing the number 1 to the boolean value
  \c{true} returns true. If you want to perform a comparison
  without such implicit value conversion, use strictlyEquals().

  Note that if this QJSValue or the \a other value are objects,
  calling this function has side effects on the script engine, since
  the engine will call the object's valueOf() function (and possibly
  toString()) in an attempt to convert the object to a primitive value
  (possibly resulting in an uncaught script exception).

  \sa strictlyEquals()
*/
bool QJSValue::equals(const QJSValue& other) const
{
    QV4::Value s1, s2;
    QV4::Value *v = QJSValuePrivate::valueForData(this, &s1);
    QV4::Value *ov = QJSValuePrivate::valueForData(&other, &s2);

    if (!v) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        Q_ASSERT(variant);
        if (!ov)
            return *variant == *QJSValuePrivate::getVariant(&other);
        if (variant->type() == QVariant::Map || variant->type() == QVariant::List)
            return false;
        return js_equal(variant->toString(), *ov);
        }
    if (!ov)
        return other.equals(*this);

    return Runtime::method_compareEqual(*v, *ov);
}

/*!
  Returns true if this QJSValue is equal to \a other using strict
  comparison (no conversion), otherwise returns false. The comparison
  follows the behavior described in \l{ECMA-262} section 11.9.6, "The
  Strict Equality Comparison Algorithm".

  If the type of this QJSValue is different from the type of the
  \a other value, this function returns false. If the types are equal,
  the result depends on the type, as shown in the following table:

    \table
    \header \li Type \li Result
    \row    \li Undefined  \li true
    \row    \li Null       \li true
    \row    \li Boolean    \li true if both values are true, false otherwise
    \row    \li Number     \li false if either value is NaN (Not-a-Number); true if values are equal, false otherwise
    \row    \li String     \li true if both values are exactly the same sequence of characters, false otherwise
    \row    \li Object     \li true if both values refer to the same object, false otherwise
    \endtable

  \sa equals()
*/
bool QJSValue::strictlyEquals(const QJSValue& other) const
{
    QV4::Value s1, s2;
    QV4::Value *v = QJSValuePrivate::valueForData(this, &s1);
    QV4::Value *ov = QJSValuePrivate::valueForData(&other, &s2);

    if (!v) {
        QVariant *variant = QJSValuePrivate::getVariant(this);
        Q_ASSERT(variant);
        if (!ov)
            return *variant == *QJSValuePrivate::getVariant(&other);
        if (variant->type() == QVariant::Map || variant->type() == QVariant::List)
            return false;
        if (ov->isString())
            return variant->toString() == ov->stringValue()->toQString();
        return false;
    }
    if (!ov)
        return other.strictlyEquals(*this);

    return RuntimeHelpers::strictEqual(*v, *ov);
}

/*!
  Returns the value of this QJSValue's property with the given \a name.
  If no such property exists, an undefined QJSValue is returned.

  If the property is implemented using a getter function (i.e. has the
  PropertyGetter flag set), calling property() has side-effects on the
  script engine, since the getter function will be called (possibly
  resulting in an uncaught script exception). If an exception
  occurred, property() returns the value that was thrown (typically
  an \c{Error} object).

  \sa setProperty(), hasProperty(), QJSValueIterator
*/
QJSValue QJSValue::property(const QString& name) const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return QJSValue();

    QV4::Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return QJSValue();

    ScopedString s(scope, engine->newString(name));
    uint idx = s->asArrayIndex();
    if (idx < UINT_MAX)
        return property(idx);

    s->makeIdentifier(engine);
    QV4::ScopedValue result(scope, o->get(s));
    if (engine->hasException)
        result = engine->catchException();

    return QJSValue(engine, result->asReturnedValue());
}

/*!
  \overload

  Returns the property at the given \a arrayIndex.

  This function is provided for convenience and performance when
  working with array objects.

  If this QJSValue is not an Array object, this function behaves
  as if property() was called with the string representation of \a
  arrayIndex.
*/
QJSValue QJSValue::property(quint32 arrayIndex) const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return QJSValue();

    QV4::Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return QJSValue();

    QV4::ScopedValue result(scope, arrayIndex == UINT_MAX ? o->get(engine->id_uintMax()) : o->getIndexed(arrayIndex));
    if (engine->hasException)
        engine->catchException();
    return QJSValue(engine, result->asReturnedValue());
}

/*!
  Sets the value of this QJSValue's property with the given \a name to
  the given \a value.

  If this QJSValue is not an object, this function does nothing.

  If this QJSValue does not already have a property with name \a name,
  a new property is created.

  \sa property(), deleteProperty()
*/
void QJSValue::setProperty(const QString& name, const QJSValue& value)
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return;
    Scope scope(engine);

    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return;

    if (!QJSValuePrivate::checkEngine(engine, value)) {
        qWarning("QJSValue::setProperty(%s) failed: cannot set value created in a different engine", name.toUtf8().constData());
        return;
    }

    ScopedString s(scope, engine->newString(name));
    uint idx = s->asArrayIndex();
    if (idx < UINT_MAX) {
        setProperty(idx, value);
        return;
    }

    s->makeIdentifier(scope.engine);
    QV4::ScopedValue v(scope, QJSValuePrivate::convertedToValue(engine, value));
    o->put(s, v);
    if (engine->hasException)
        engine->catchException();
}

/*!
  \overload

  Sets the property at the given \a arrayIndex to the given \a value.

  This function is provided for convenience and performance when
  working with array objects.

  If this QJSValue is not an Array object, this function behaves
  as if setProperty() was called with the string representation of \a
  arrayIndex.
*/
void QJSValue::setProperty(quint32 arrayIndex, const QJSValue& value)
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return;
    Scope scope(engine);

    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return;

    if (!QJSValuePrivate::checkEngine(engine, value)) {
        qWarning("QJSValue::setProperty(%d) failed: cannot set value created in a different engine", arrayIndex);
        return;
    }

    QV4::ScopedValue v(scope, QJSValuePrivate::convertedToValue(engine, value));
    if (arrayIndex != UINT_MAX)
        o->putIndexed(arrayIndex, v);
    else
        o->put(engine->id_uintMax(), v);
    if (engine->hasException)
        engine->catchException();
}

/*!
  Attempts to delete this object's property of the given \a name.
  Returns true if the property was deleted, otherwise returns false.

  The behavior of this function is consistent with the JavaScript
  delete operator. In particular:

  \list
  \li Non-configurable properties cannot be deleted.
  \li This function will return true even if this object doesn't
     have a property of the given \a name (i.e., non-existent
     properties are "trivially deletable").
  \li If this object doesn't have an own property of the given
     \a name, but an object in the prototype() chain does, the
     prototype object's property is not deleted, and this function
     returns true.
  \endlist

  \sa setProperty(), hasOwnProperty()
*/
bool QJSValue::deleteProperty(const QString &name)
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return false;

    Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return false;

    ScopedString s(scope, engine->newString(name));
    bool b = o->deleteProperty(s);
    if (engine->hasException)
        engine->catchException();
    return b;
}

/*!
  Returns true if this object has a property of the given \a name,
  otherwise returns false.

  \sa property(), hasOwnProperty()
*/
bool QJSValue::hasProperty(const QString &name) const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return false;

    Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return false;

    ScopedString s(scope, engine->newIdentifier(name));
    return o->hasProperty(s);
}

/*!
  Returns true if this object has an own (not prototype-inherited)
  property of the given \a name, otherwise returns false.

  \sa property(), hasProperty()
*/
bool QJSValue::hasOwnProperty(const QString &name) const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return false;

    Scope scope(engine);
    ScopedObject o(scope, QJSValuePrivate::getValue(this));
    if (!o)
        return false;

    ScopedString s(scope, engine->newIdentifier(name));
    return o->hasOwnProperty(s);
}

/*!
 * If this QJSValue is a QObject, returns the QObject pointer
 * that the QJSValue represents; otherwise, returns 0.
 *
 * If the QObject that this QJSValue wraps has been deleted,
 * this function returns 0 (i.e. it is possible for toQObject()
 * to return 0 even when isQObject() returns true).
 *
 * \sa isQObject()
 */
QObject *QJSValue::toQObject() const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return 0;
    QV4::Scope scope(engine);
    QV4::Scoped<QV4::QObjectWrapper> wrapper(scope, QJSValuePrivate::getValue(this));
    if (!wrapper)
        return 0;

    return wrapper->object();
}

/*!
  \since 5.8

 * If this QJSValue is a QMetaObject, returns the QMetaObject pointer
 * that the QJSValue represents; otherwise, returns 0.
 *
 * \sa isQMetaObject()
 */
const QMetaObject *QJSValue::toQMetaObject() const
{
    QV4::ExecutionEngine *engine = QJSValuePrivate::engine(this);
    if (!engine)
        return 0;
    QV4::Scope scope(engine);
    QV4::Scoped<QV4::QMetaObjectWrapper> wrapper(scope, QJSValuePrivate::getValue(this));
    if (!wrapper)
        return 0;

    return wrapper->metaObject();
}


/*!
  Returns a QDateTime representation of this value, in local time.
  If this QJSValue is not a date, or the value of the date is NaN
  (Not-a-Number), an invalid QDateTime is returned.

  \sa isDate()
*/
QDateTime QJSValue::toDateTime() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    if (val) {
        QV4::DateObject *date = val->as<DateObject>();
        if (date)
            return date->toQDateTime();
    }
    return QDateTime();
}

/*!
  Returns true if this QJSValue is an object of the Date class;
  otherwise returns false.
*/
bool QJSValue::isDate() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    return val && val->as<DateObject>();
}

/*!
  Returns true if this QJSValue is an object of the RegExp class;
  otherwise returns false.
*/
bool QJSValue::isRegExp() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    return val && val->as<RegExpObject>();
}

/*!
  Returns true if this QJSValue is a QObject; otherwise returns
  false.

  Note: This function returns true even if the QObject that this
  QJSValue wraps has been deleted.

  \sa toQObject(), QJSEngine::newQObject()
*/
bool QJSValue::isQObject() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    return val && val->as<QV4::QObjectWrapper>() != 0;
}

/*!
  \since 5.8

  Returns true if this QJSValue is a QMetaObject; otherwise returns
  false.

  \sa toQMetaObject(), QJSEngine::newQMetaObject()
*/
bool QJSValue::isQMetaObject() const
{
    QV4::Value *val = QJSValuePrivate::getValue(this);
    return val && val->as<QV4::QMetaObjectWrapper>() != 0;
}

QT_END_NAMESPACE
