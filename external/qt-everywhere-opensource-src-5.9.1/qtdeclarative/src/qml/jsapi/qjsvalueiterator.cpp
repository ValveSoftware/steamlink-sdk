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

#include "qjsvalueiterator.h"
#include "qjsvalueiterator_p.h"
#include "qjsvalue_p.h"
#include "private/qv4string_p.h"
#include "private/qv4object_p.h"
#include "private/qv4context_p.h"

QT_BEGIN_NAMESPACE

QJSValueIteratorPrivate::QJSValueIteratorPrivate(const QJSValue &v)
    : value(v)
    , currentIndex(UINT_MAX)
    , nextIndex(UINT_MAX)
{
    QV4::ExecutionEngine *e = QJSValuePrivate::engine(&v);
    if (!e)
        return;

    QV4::Scope scope(e);
    QV4::ScopedObject o(scope, QJSValuePrivate::getValue(&v));
    iterator.set(e, e->newForEachIteratorObject(o));
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
    it->d()->it().flags =  QV4::ObjectIterator::NoFlags;
    QV4::ScopedString nm(scope);
    QV4::Property nextProperty;
    QV4::PropertyAttributes nextAttributes;
    it->d()->it().next(nm.getRef(), &d_ptr->nextIndex, &nextProperty, &nextAttributes);
    d_ptr->nextName.set(v4, nm.asReturnedValue());
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
    QV4::Value *val = QJSValuePrivate::getValue(&d_ptr->value);
    if (!val || !val->isObject())
        return false;
    return d_ptr->nextName.as<QV4::String>() || d_ptr->nextIndex != UINT_MAX;
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
    QV4::Value *val = QJSValuePrivate::getValue(&d_ptr->value);
    if (!val || !val->isObject())
        return false;
    d_ptr->currentName = d_ptr->nextName;
    d_ptr->currentIndex = d_ptr->nextIndex;

    QV4::ExecutionEngine *v4 = d_ptr->iterator.engine();
    if (!v4)
        return false;
    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ForEachIteratorObject> it(scope, d_ptr->iterator.value());
    QV4::ScopedString nm(scope);
    QV4::Property nextProperty;
    QV4::PropertyAttributes nextAttributes;
    it->d()->it().next(nm.getRef(), &d_ptr->nextIndex, &nextProperty, &nextAttributes);
    d_ptr->nextName.set(v4, nm.asReturnedValue());
    return d_ptr->currentName.as<QV4::String>() || d_ptr->currentIndex != UINT_MAX;
}

/*!
    Returns the name of the last property that was jumped over using
    next().

    \sa value()
*/
QString QJSValueIterator::name() const
{
    QV4::Value *val = QJSValuePrivate::getValue(&d_ptr->value);
    if (!val || !val->isObject())
        return QString();
    if (QV4::String *s = d_ptr->currentName.as<QV4::String>())
        return s->toQString();
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
    QV4::ScopedObject obj(scope, QJSValuePrivate::getValue(&d_ptr->value));
    if (!obj)
        return QJSValue();

    if (!d_ptr->currentName.as<QV4::String>() && d_ptr->currentIndex == UINT_MAX)
        return QJSValue();

    QV4::ScopedValue v(scope, d_ptr->currentIndex == UINT_MAX ? obj->get(d_ptr->currentName.as<QV4::String>()) : obj->getIndexed(d_ptr->currentIndex));
    if (scope.hasException()) {
        engine->catchException();
        return QJSValue();
    }
    return QJSValue(engine, v->asReturnedValue());
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
    d_ptr->currentName.clear();
    d_ptr->nextName.clear();
    QV4::ExecutionEngine *v4 = d_ptr->iterator.engine();
    if (!v4) {
        d_ptr->iterator.clear();
        return *this;
    }

    QV4::Scope scope(v4);
    QV4::ScopedObject o(scope, QJSValuePrivate::getValue(&object));
    d_ptr->iterator.set(v4, v4->newForEachIteratorObject(o));
    QV4::Scoped<QV4::ForEachIteratorObject> it(scope, d_ptr->iterator.value());
    it->d()->it().flags =  QV4::ObjectIterator::NoFlags;
    QV4::ScopedString nm(scope);
    QV4::Property nextProperty;
    QV4::PropertyAttributes nextAttributes;
    it->d()->it().next(nm.getRef(), &d_ptr->nextIndex, &nextProperty, &nextAttributes);
    d_ptr->nextName.set(v4, nm.asReturnedValue());
    return *this;
}

QT_END_NAMESPACE
