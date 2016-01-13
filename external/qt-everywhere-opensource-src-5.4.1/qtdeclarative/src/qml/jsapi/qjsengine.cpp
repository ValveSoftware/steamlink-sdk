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

#include "qjsengine.h"
#include "qjsengine_p.h"
#include "qjsvalue.h"
#include "qjsvalue_p.h"
#include "private/qv8engine_p.h"

#include "private/qv4engine_p.h"
#include "private/qv4mm_p.h"
#include "private/qv4globalobject_p.h"
#include "private/qv4script_p.h"
#include "private/qv4runtime_p.h"
#include <private/qqmlbuiltinfunctions_p.h>

#include <QtCore/qdatetime.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtCore/qdatetime.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qpluginloader.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <private/qqmlglobal_p.h>
#include <qqmlengine.h>

#undef Q_D
#undef Q_Q
#define Q_D(blah)
#define Q_Q(blah)

Q_DECLARE_METATYPE(QList<int>)

/*!
  \since 5.0
  \class QJSEngine
  \reentrant

  \brief The QJSEngine class provides an environment for evaluating JavaScript code.

  \ingroup qtjavascript
  \inmodule QtQml
  \mainclass

  \section1 Evaluating Scripts

  Use evaluate() to evaluate script code.

  \snippet code/src_script_qjsengine.cpp 0

  evaluate() returns a QJSValue that holds the result of the
  evaluation. The QJSValue class provides functions for converting
  the result to various C++ types (e.g. QJSValue::toString()
  and QJSValue::toNumber()).

  The following code snippet shows how a script function can be
  defined and then invoked from C++ using QJSValue::call():

  \snippet code/src_script_qjsengine.cpp 1

  As can be seen from the above snippets, a script is provided to the
  engine in the form of a string. One common way of loading scripts is
  by reading the contents of a file and passing it to evaluate():

  \snippet code/src_script_qjsengine.cpp 2

  Here we pass the name of the file as the second argument to
  evaluate().  This does not affect evaluation in any way; the second
  argument is a general-purpose string that is used to identify the
  script for debugging purposes.

  \section1 Engine Configuration

  The globalObject() function returns the \b {Global Object}
  associated with the script engine. Properties of the Global Object
  are accessible from any script code (i.e. they are global
  variables). Typically, before evaluating "user" scripts, you will
  want to configure a script engine by adding one or more properties
  to the Global Object:

  \snippet code/src_script_qjsengine.cpp 3

  Adding custom properties to the scripting environment is one of the
  standard means of providing a scripting API that is specific to your
  application. Usually these custom properties are objects created by
  the newQObject() or newObject() functions.

  \section1 Script Exceptions

  evaluate() can throw a script exception (e.g. due to a syntax
  error); in that case, the return value is the value that was thrown
  (typically an \c{Error} object). You can check whether the
  evaluation caused an exception by calling isError() on the return
  value. If isError() returns true, you can call toString() on the
  error object to obtain an error message.

  \snippet code/src_script_qjsengine.cpp 4

  \section1 Script Object Creation

  Use newObject() to create a JavaScript object; this is the
  C++ equivalent of the script statement \c{new Object()}. You can use
  the object-specific functionality in QJSValue to manipulate the
  script object (e.g. QJSValue::setProperty()). Similarly, use
  newArray() to create a JavaScript array object.

  \section1 QObject Integration

  Use newQObject() to wrap a QObject (or subclass)
  pointer. newQObject() returns a proxy script object; properties,
  children, and signals and slots of the QObject are available as
  properties of the proxy object. No binding code is needed because it
  is done dynamically using the Qt meta object system.

  \snippet code/src_script_qjsengine.cpp 5

  \sa QJSValue, {Making Applications Scriptable}

*/

QT_BEGIN_NAMESPACE


/*!
    Constructs a QJSEngine object.

    The globalObject() is initialized to have properties as described in
    \l{ECMA-262}, Section 15.1.
*/
QJSEngine::QJSEngine()
    : d(new QV8Engine(this))
{
}

/*!
    Constructs a QJSEngine object with the given \a parent.

    The globalObject() is initialized to have properties as described in
    \l{ECMA-262}, Section 15.1.
*/

QJSEngine::QJSEngine(QObject *parent)
    : QObject(parent)
    , d(new QV8Engine(this))
{
}

/*!
    \internal
*/
QJSEngine::QJSEngine(QJSEnginePrivate &dd, QObject *parent)
    : QObject(dd, parent)
    , d(new QV8Engine(this))
{
}

/*!
    Destroys this QJSEngine.

    Garbage is not collected from the persistent JS heap during QJSEngine
    destruction. If you need all memory freed, call collectGarbage manually
    right before destroying the QJSEngine.
*/
QJSEngine::~QJSEngine()
{
    delete d;
}

/*!
    \fn QV8Engine *QJSEngine::handle() const
    \internal
*/

/*!
    Runs the garbage collector.

    The garbage collector will attempt to reclaim memory by locating and disposing of objects that are
    no longer reachable in the script environment.

    Normally you don't need to call this function; the garbage collector will automatically be invoked
    when the QJSEngine decides that it's wise to do so (i.e. when a certain number of new objects
    have been created). However, you can call this function to explicitly request that garbage
    collection should be performed as soon as possible.
*/
void QJSEngine::collectGarbage()
{
    d->m_v4Engine->memoryManager->runGC();
}

/*!
  \since 5.4

  Installs translator functions on the given \a object, or on the Global
  Object if no object is specified.

  The relation between script translator functions and C++ translator
  functions is described in the following table:

    \table
    \header \li Script Function \li Corresponding C++ Function
    \row    \li qsTr()       \li QObject::tr()
    \row    \li QT_TR_NOOP() \li QT_TR_NOOP()
    \row    \li qsTranslate() \li QCoreApplication::translate()
    \row    \li QT_TRANSLATE_NOOP() \li QT_TRANSLATE_NOOP()
    \row    \li qsTrId() \li qtTrId()
    \row    \li QT_TRID_NOOP() \li QT_TRID_NOOP()
    \endtable

  It also adds an arg() method to the string prototype.

  \sa {Internationalization with Qt}
*/
void QJSEngine::installTranslatorFunctions(const QJSValue &object)
{
    QV4::ExecutionEngine *v4 = d->m_v4Engine;
    QV4::Scope scope(v4);
    QJSValuePrivate *vp = QJSValuePrivate::get(object);
    QV4::ScopedObject obj(scope, vp->getValue(v4));
    if (!obj)
        obj = v4->globalObject;
#ifndef QT_NO_TRANSLATION
    obj->defineDefaultProperty(QStringLiteral("qsTranslate"), QV4::GlobalExtensions::method_qsTranslate);
    obj->defineDefaultProperty(QStringLiteral("QT_TRANSLATE_NOOP"), QV4::GlobalExtensions::method_qsTranslateNoOp);
    obj->defineDefaultProperty(QStringLiteral("qsTr"), QV4::GlobalExtensions::method_qsTr);
    obj->defineDefaultProperty(QStringLiteral("QT_TR_NOOP"), QV4::GlobalExtensions::method_qsTrNoOp);
    obj->defineDefaultProperty(QStringLiteral("qsTrId"), QV4::GlobalExtensions::method_qsTrId);
    obj->defineDefaultProperty(QStringLiteral("QT_TRID_NOOP"), QV4::GlobalExtensions::method_qsTrIdNoOp);

    // string prototype extension
    v4->stringObjectClass->prototype->defineDefaultProperty(QStringLiteral("arg"),
                                                            QV4::GlobalExtensions::method_string_arg);
#endif
}

/*!
    Evaluates \a program, using \a lineNumber as the base line number,
    and returns the result of the evaluation.

    The script code will be evaluated in the context of the global object.

    The evaluation of \a program can cause an exception in the
    engine; in this case the return value will be the exception
    that was thrown (typically an \c{Error} object; see
    QJSValue::isError()).

    \a lineNumber is used to specify a starting line number for \a
    program; line number information reported by the engine that pertains
    to this evaluation will be based on this argument. For example, if
    \a program consists of two lines of code, and the statement on the
    second line causes a script exception, the exception line number
    would be \a lineNumber plus one. When no starting line number is
    specified, line numbers will be 1-based.

    \a fileName is used for error reporting. For example, in error objects
    the file name is accessible through the "fileName" property if it is
    provided with this function.

    \note If an exception was thrown and the exception value is not an
    Error instance (i.e., QJSValue::isError() returns \c false), the
    exception value will still be returned, but there is currently no
    API for detecting that an exception did occur in this case.
*/
QJSValue QJSEngine::evaluate(const QString& program, const QString& fileName, int lineNumber)
{
    QV4::ExecutionEngine *v4 = d->m_v4Engine;
    QV4::Scope scope(v4);
    QV4::ExecutionContext *ctx = v4->currentContext();
    if (ctx != v4->rootContext)
        ctx = v4->pushGlobalContext();
    QV4::ScopedValue result(scope);

    QV4::Script script(ctx, program, fileName, lineNumber);
    script.strictMode = ctx->d()->strictMode;
    script.inheritContext = true;
    script.parse();
    if (!scope.engine->hasException)
        result = script.run();
    if (scope.engine->hasException)
        result = ctx->catchException();
    if (ctx != v4->rootContext)
        v4->popContext();
    return new QJSValuePrivate(v4, result);
}

/*!
  Creates a JavaScript object of class Object.

  The prototype of the created object will be the Object
  prototype object.

  \sa newArray(), QJSValue::setProperty()
*/
QJSValue QJSEngine::newObject()
{
    QV4::Scope scope(d->m_v4Engine);
    QV4::ScopedValue v(scope, d->m_v4Engine->newObject());
    return new QJSValuePrivate(d->m_v4Engine, v);
}

/*!
  Creates a JavaScript object of class Array with the given \a length.

  \sa newObject()
*/
QJSValue QJSEngine::newArray(uint length)
{
    QV4::Scope scope(d->m_v4Engine);
    QV4::Scoped<QV4::ArrayObject> array(scope, d->m_v4Engine->newArrayObject());
    if (length < 0x1000)
        array->arrayReserve(length);
    array->setArrayLengthUnchecked(length);
    return new QJSValuePrivate(d->m_v4Engine, array);
}

/*!
  Creates a JavaScript object that wraps the given QObject \a
  object, using JavaScriptOwnership.

  Signals and slots, properties and children of \a object are
  available as properties of the created QJSValue.

  If \a object is a null pointer, this function returns a null value.

  If a default prototype has been registered for the \a object's class
  (or its superclass, recursively), the prototype of the new script
  object will be set to be that default prototype.

  If the given \a object is deleted outside of the engine's control, any
  attempt to access the deleted QObject's members through the JavaScript
  wrapper object (either by script code or C++) will result in a
  script exception.

  \sa QJSValue::toQObject()
*/
QJSValue QJSEngine::newQObject(QObject *object)
{
    Q_D(QJSEngine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(d);
    QV4::Scope scope(v4);
    if (object) {
        QQmlData *ddata = QQmlData::get(object, true);
        if (!ddata || !ddata->explicitIndestructibleSet)
            QQmlEngine::setObjectOwnership(object, QQmlEngine::JavaScriptOwnership);
    }
    QV4::ScopedValue v(scope, QV4::QObjectWrapper::wrap(v4, object));
    return new QJSValuePrivate(v4, v);
}

/*!
  Returns this engine's Global Object.

  By default, the Global Object contains the built-in objects that are
  part of \l{ECMA-262}, such as Math, Date and String. Additionally,
  you can set properties of the Global Object to make your own
  extensions available to all script code. Non-local variables in
  script code will be created as properties of the Global Object, as
  well as local variables in global code.
*/
QJSValue QJSEngine::globalObject() const
{
    Q_D(const QJSEngine);
    QV4::Scope scope(d->m_v4Engine);
    QV4::ScopedValue v(scope, d->m_v4Engine->globalObject);
    return new QJSValuePrivate(d->m_v4Engine, v);
}

/*!
 *  \internal
 * used by QJSEngine::toScriptValue
 */
QJSValue QJSEngine::create(int type, const void *ptr)
{
    Q_D(QJSEngine);
    QV4::Scope scope(d->m_v4Engine);
    QV4::ScopedValue v(scope, d->metaTypeToJS(type, ptr));
    return new QJSValuePrivate(d->m_v4Engine, v);
}

/*!
    \internal
    convert \a value to \a type, store the result in \a ptr
*/
bool QJSEngine::convertV2(const QJSValue &value, int type, void *ptr)
{
    QJSValuePrivate *vp = QJSValuePrivate::get(value);
    QV8Engine *engine = vp->engine ? vp->engine->v8Engine : 0;
    if (engine) {
        QV4::Scope scope(engine->m_v4Engine);
        QV4::ScopedValue v(scope, vp->getValue(engine->m_v4Engine));
        return engine->metaTypeFromJS(v, type, ptr);
    } else if (vp->value.isEmpty()) {
        if (vp->unboundData.userType() == QMetaType::QString) {
            QString string = vp->unboundData.toString();
            // have a string based value without engine. Do conversion manually
            if (type == QMetaType::Bool) {
                *reinterpret_cast<bool*>(ptr) = string.length() != 0;
                return true;
            }
            if (type == QMetaType::QString) {
                *reinterpret_cast<QString*>(ptr) = string;
                return true;
            }
            double d = QV4::RuntimeHelpers::stringToNumber(string);
            switch (type) {
            case QMetaType::Int:
                *reinterpret_cast<int*>(ptr) = QV4::Primitive::toInt32(d);
                return true;
            case QMetaType::UInt:
                *reinterpret_cast<uint*>(ptr) = QV4::Primitive::toUInt32(d);
                return true;
            case QMetaType::LongLong:
                *reinterpret_cast<qlonglong*>(ptr) = QV4::Primitive::toInteger(d);
                return true;
            case QMetaType::ULongLong:
                *reinterpret_cast<qulonglong*>(ptr) = QV4::Primitive::toInteger(d);
                return true;
            case QMetaType::Double:
                *reinterpret_cast<double*>(ptr) = d;
                return true;
            case QMetaType::Float:
                *reinterpret_cast<float*>(ptr) = d;
                return true;
            case QMetaType::Short:
                *reinterpret_cast<short*>(ptr) = QV4::Primitive::toInt32(d);
                return true;
            case QMetaType::UShort:
                *reinterpret_cast<unsigned short*>(ptr) = QV4::Primitive::toUInt32(d);
                return true;
            case QMetaType::Char:
                *reinterpret_cast<char*>(ptr) = QV4::Primitive::toInt32(d);
                return true;
            case QMetaType::UChar:
                *reinterpret_cast<unsigned char*>(ptr) = QV4::Primitive::toUInt32(d);
                return true;
            case QMetaType::QChar:
                *reinterpret_cast<QChar*>(ptr) = QV4::Primitive::toUInt32(d);
                return true;
            default:
                return false;
            }
        } else {
            return QMetaType::convert(&vp->unboundData.data_ptr(), vp->unboundData.userType(), ptr, type);
        }
    } else {
        switch (type) {
            case QMetaType::Bool:
                *reinterpret_cast<bool*>(ptr) = vp->value.toBoolean();
                return true;
            case QMetaType::Int:
                *reinterpret_cast<int*>(ptr) = vp->value.toInt32();
                return true;
            case QMetaType::UInt:
                *reinterpret_cast<uint*>(ptr) = vp->value.toUInt32();
                return true;
            case QMetaType::LongLong:
                *reinterpret_cast<qlonglong*>(ptr) = vp->value.toInteger();
                return true;
            case QMetaType::ULongLong:
                *reinterpret_cast<qulonglong*>(ptr) = vp->value.toInteger();
                return true;
            case QMetaType::Double:
                *reinterpret_cast<double*>(ptr) = vp->value.toNumber();
                return true;
            case QMetaType::QString:
                *reinterpret_cast<QString*>(ptr) = value.toString();
                return true;
            case QMetaType::Float:
                *reinterpret_cast<float*>(ptr) = vp->value.toNumber();
                return true;
            case QMetaType::Short:
                *reinterpret_cast<short*>(ptr) = vp->value.toInt32();
                return true;
            case QMetaType::UShort:
                *reinterpret_cast<unsigned short*>(ptr) = vp->value.toUInt16();
                return true;
            case QMetaType::Char:
                *reinterpret_cast<char*>(ptr) = vp->value.toInt32();
                return true;
            case QMetaType::UChar:
                *reinterpret_cast<unsigned char*>(ptr) = vp->value.toUInt16();
                return true;
            case QMetaType::QChar:
                *reinterpret_cast<QChar*>(ptr) = vp->value.toUInt16();
                return true;
            default:
                return false;
        }
    }
}

/*! \fn QJSValue QJSEngine::toScriptValue(const T &value)

    Creates a QJSValue with the given \a value.

    \sa fromScriptValue()
*/

/*! \fn T QJSEngine::fromScriptValue(const QJSValue &value)

    Returns the given \a value converted to the template type \c{T}.

    \sa toScriptValue()
*/

QT_END_NAMESPACE

#include "moc_qjsengine.cpp"
