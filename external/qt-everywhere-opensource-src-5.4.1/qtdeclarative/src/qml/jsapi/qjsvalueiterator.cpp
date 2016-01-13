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

#include "qjsvalueiterator.h"
#include "qjsvalueiterator_p.h"
#include "qjsvalue_p.h"
#include "private/qv4string_p.h"
#include "private/qv4object_p.h"

QT_BEGIN_NAMESPACE

QJSValueIteratorPrivate::QJSValueIteratorPrivate(const QJSValue &v)
    : value(v)
    , currentIndex(UINT_MAX)
    , nextIndex(UINT_MAX)
{
    QJSValuePrivate *jsp = QJSValuePrivate::get(value);
    QV4::ExecutionEngine *e = jsp->engine;
    if (!e)
        return;

    QV4::Scope scope(e);
    QV4::ScopedObject o(scope, jsp->value);
    iterator = e->newForEachIteratorObject(e->currentContext(), o)->asReturnedValue();

    currentName = (QV4::String *)0;
    nextName = (QV4::String *)0;
}


/*!
    \class QJSValueIterator

    \brief The QJSValueIterator class provides a Java-style iterator for QJSValue.

    \ingroup qtjavascript
    \inmodule QtQml


    The QJSValueIterator constructor takes a QJSValue as
    argument.  After construction, the iterator is located at the very
    beginning of the sequence of properties. Here's how to iterate over
    all the properties of a QJSValue:

    \snippet code/src_script_qjsvalueiterator.cpp 0

    The next() advances the iterator. The name() and value()
    functions return the name and value of the last item that was
    jumped over.

    Note that QJSValueIterator only iterates over the QJSValue's
    own properties; i.e. it does not follow the prototype chain. You can
    use a loop like this to follow the prototype chain:

    \snippet code/src_script_qjsvalueiterator.cpp 1

    \sa QJSValue::property()
*/

/*!
    Constructs an iterator for traversing \a object. The iterator is
    set to be at the front of the sequence of properties (before the
    first property).
*/
QJSValueIterator::QJSValueIterator(const QJSValue& object)
    : d_ptr(new QJSValueIteratorPrivate(object))
{
    QV4::ExecutionEngine *v4 = d_ptr->iterator.engine();
    if (!v4)
        return;
    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ForEachIteratorObject> it(scope, d_ptr->iterator.value());
    it->d()->it.flags =  QV4::ObjectIterator::NoFlags;
    QV4::String *nm = 0;
    it->d()->it.next(nm, &d_ptr->nextIndex, &d_ptr->nextProperty, &d_ptr->nextAttributes);
    d_ptr->nextName = nm;
}

/*!
    Destroys the iterator.
*/
QJSValueIterator::~QJSValueIterator()
{
}

/*!
    Returns true if there is at least one item ahead of the iterator
    (i.e. the iterator is \e not at the back of the property sequence);
    otherwise returns false.

    \sa next()
*/
bool QJSValueIterator::hasNext() const
{
    if (!QJSValuePrivate::get(d_ptr->value)->value.isObject())
        return false;
    return !!d_ptr->nextName || d_ptr->nextIndex != UINT_MAX;
}

/*!
    Advances the iterator by one position.
    Returns true if there was at least one item ahead of the iterator
    (i.e. the iterator was \e not already at the back of the property sequence);
    otherwise returns false.

    \sa hasNext(), name()
*/
bool QJSValueIterator::next()
{
    if (!QJSValuePrivate::get(d_ptr->value)->value.isObject())
        return false;
    d_ptr->currentName = d_ptr->nextName;
    d_ptr->currentIndex = d_ptr->nextIndex;
    d_ptr->currentProperty.copy(d_ptr->nextProperty, d_ptr->nextAttributes);
    d_ptr->currentAttributes = d_ptr->nextAttributes;

    QV4::ExecutionEngine *v4 = d_ptr->iterator.engine();
    if (!v4)
        return false;
    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ForEachIteratorObject> it(scope, d_ptr->iterator.value());
    QV4::String *nm = 0;
    it->d()->it.next(nm, &d_ptr->nextIndex, &d_ptr->nextProperty, &d_ptr->nextAttributes);
    d_ptr->nextName = nm;
    return !!d_ptr->currentName || d_ptr->currentIndex != UINT_MAX;
}

/*!
    Returns the name of the last property that was jumped over using
    next().

    \sa value()
*/
QString QJSValueIterator::name() const
{
    if (!QJSValuePrivate::get(d_ptr->value)->value.isObject())
        return QString();
    if (!!d_ptr->currentName)
        return d_ptr->currentName->toQString();
    if (d_ptr->currentIndex < UINT_MAX)
        return QString::number(d_ptr->currentIndex);
    return QString();
}


/*!
    Returns the value of the last property that was jumped over using
    next().

    \sa name()
*/
QJSValue QJSValueIterator::value() const
{
    QV4::ExecutionEngine *engine = d_ptr->iterator.engine();
    if (!engine)
        return QJSValue();
    QV4::Scope scope(engine);
    QV4::ScopedObject obj(scope, QJSValuePrivate::get(d_ptr->value)->value);
    if (!obj)
        return QJSValue();

    QV4::ExecutionContext *ctx = engine->currentContext();
    if (!d_ptr->currentName && d_ptr->currentIndex == UINT_MAX)
        return QJSValue();

    QV4::ScopedValue v(scope, obj->getValue(obj, &d_ptr->currentProperty, d_ptr->currentAttributes));
    if (scope.hasException()) {
        ctx->catchException();
        return QJSValue();
    }
    return new QJSValuePrivate(engine, v);
}


/*!
    Makes the iterator operate on \a object. The iterator is set to be
    at the front of the sequence of properties (before the first
    property).
*/
QJSValueIterator& QJSValueIterator::operator=(QJSValue& object)
{
    d_ptr->value = object;
    d_ptr->currentIndex = UINT_MAX;
    d_ptr->nextIndex = UINT_MAX;
    d_ptr->currentName = (QV4::String *)0;
    d_ptr->nextName = (QV4::String *)0;
    QV4::ExecutionEngine *v4 = d_ptr->iterator.engine();
    if (!v4) {
        d_ptr->iterator = QV4::Encode::undefined();
        return *this;
    }

    QJSValuePrivate *jsp = QJSValuePrivate::get(object);
    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, jsp->value);
    d_ptr->iterator = v4->newForEachIteratorObject(v4->currentContext(), o)->asReturnedValue();
    QV4::Scoped<QV4::ForEachIteratorObject> it(scope, d_ptr->iterator.value());
    it->d()->it.flags =  QV4::ObjectIterator::NoFlags;
    QV4::String *nm = 0;
    it->d()->it.next(nm, &d_ptr->nextIndex, &d_ptr->nextProperty, &d_ptr->nextAttributes);
    d_ptr->nextName = nm;
    return *this;
}

QT_END_NAMESPACE
