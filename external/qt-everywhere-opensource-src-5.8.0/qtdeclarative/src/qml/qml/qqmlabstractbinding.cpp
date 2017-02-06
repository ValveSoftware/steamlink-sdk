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

#include "qqmlabstractbinding_p.h"

#include <QtQml/qqmlinfo.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlvaluetypeproxybinding_p.h>

QT_BEGIN_NAMESPACE

QQmlAbstractBinding::QQmlAbstractBinding()
    : m_targetIndex(-1)
{
    Q_ASSERT(!isAddedToObject());
}

QQmlAbstractBinding::~QQmlAbstractBinding()
{
    Q_ASSERT(!ref);
    Q_ASSERT(!isAddedToObject());

    if (m_nextBinding.data() && !m_nextBinding->ref.deref())
        delete m_nextBinding.data();
}

/*!
Add this binding to \a object.

This transfers ownership of the binding to the object, marks the object's property as
being bound.

However, it does not enable the binding itself or call update() on it.
*/
void QQmlAbstractBinding::addToObject()
{
    Q_ASSERT(!nextBinding());
    Q_ASSERT(isAddedToObject() == false);

    QObject *obj = targetObject();
    Q_ASSERT(obj);

    QQmlData *data = QQmlData::get(obj, true);

    int coreIndex = targetPropertyIndex().coreIndex();
    if (targetPropertyIndex().hasValueTypeIndex()) {
        // Value type

        // Find the value type proxy (if there is one)
        QQmlValueTypeProxyBinding *proxy = 0;
        if (data->hasBindingBit(coreIndex)) {
            QQmlAbstractBinding *b = data->bindings;
            while (b && (b->targetPropertyIndex().coreIndex() != coreIndex ||
                         b->targetPropertyIndex().hasValueTypeIndex()))
                b = b->nextBinding();
            Q_ASSERT(b && b->isValueTypeProxy());
            proxy = static_cast<QQmlValueTypeProxyBinding *>(b);
        }

        if (!proxy) {
            proxy = new QQmlValueTypeProxyBinding(obj, QQmlPropertyIndex(coreIndex));

            Q_ASSERT(proxy->targetPropertyIndex().coreIndex() == coreIndex);
            Q_ASSERT(!proxy->targetPropertyIndex().hasValueTypeIndex());
            Q_ASSERT(proxy->targetObject() == obj);

            proxy->addToObject();
        }

        setNextBinding(proxy->m_bindings.data());
        proxy->m_bindings = this;

    } else {
        setNextBinding(data->bindings);
        if (data->bindings) {
            data->bindings->ref.deref();
            Q_ASSERT(data->bindings->ref.refCount > 0);
        }
        data->bindings = this;
        ref.ref();

        data->setBindingBit(obj, coreIndex);
    }

    setAddedToObject(true);
}

/*!
Remove the binding from the object.
*/
void QQmlAbstractBinding::removeFromObject()
{
    if (!isAddedToObject())
        return;

    setAddedToObject(false);

    QObject *obj = targetObject();
    QQmlData *data = QQmlData::get(obj, false);
    Q_ASSERT(data);

    QQmlAbstractBinding::Ptr next;
    next = nextBinding();
    setNextBinding(0);

    int coreIndex = targetPropertyIndex().coreIndex();
    if (targetPropertyIndex().hasValueTypeIndex()) {

        // Find the value type binding
        QQmlAbstractBinding *vtbinding = data->bindings;
        while (vtbinding && (vtbinding->targetPropertyIndex().coreIndex() != coreIndex ||
                             vtbinding->targetPropertyIndex().hasValueTypeIndex())) {
            vtbinding = vtbinding->nextBinding();
            Q_ASSERT(vtbinding);
        }
        Q_ASSERT(vtbinding->isValueTypeProxy());

        QQmlValueTypeProxyBinding *vtproxybinding =
            static_cast<QQmlValueTypeProxyBinding *>(vtbinding);

        QQmlAbstractBinding *binding = vtproxybinding->m_bindings.data();
        if (binding == this) {
            vtproxybinding->m_bindings = next;
        } else {
           while (binding->nextBinding() != this) {
              binding = binding->nextBinding();
              Q_ASSERT(binding);
           }
           binding->setNextBinding(next.data());
        }

        // Value type - we don't remove the proxy from the object.  It will sit their happily
        // doing nothing until it is removed by a write, a binding change or it is reused
        // to hold more sub-bindings.
        return;
    }

    if (data->bindings == this) {
        if (next.data())
            next->ref.ref();
        data->bindings = next.data();
        if (!ref.deref())
            delete this;
    } else {
        QQmlAbstractBinding *binding = data->bindings;
        while (binding->nextBinding() != this) {
            binding = binding->nextBinding();
            Q_ASSERT(binding);
        }
        binding->setNextBinding(next.data());
    }

    data->clearBindingBit(coreIndex);
}

void QQmlAbstractBinding::printBindingLoopError(QQmlProperty &prop)
{
    qmlInfo(prop.object()) << QString(QLatin1String("Binding loop detected for property \"%1\"")).arg(prop.name());
}

QString QQmlAbstractBinding::expression() const
{
    return QLatin1String("<Unknown>");
}

bool QQmlAbstractBinding::isValueTypeProxy() const
{
    return false;
}

QT_END_NAMESPACE
