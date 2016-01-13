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

#include "qqmlabstractbinding_p.h"

#include <QtQml/qqmlinfo.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlvaluetypeproxybinding_p.h>

QT_BEGIN_NAMESPACE

extern QQmlAbstractBinding::VTable QQmlBinding_vtable;
extern QQmlAbstractBinding::VTable QQmlValueTypeProxyBinding_vtable;

QQmlAbstractBinding::VTable *QQmlAbstractBinding::vTables[] = {
    &QQmlBinding_vtable,
    &QQmlValueTypeProxyBinding_vtable
};

QQmlAbstractBinding::QQmlAbstractBinding(BindingType bt)
    : m_nextBindingPtr(bt)
{
}

QQmlAbstractBinding::~QQmlAbstractBinding()
{
    Q_ASSERT(isAddedToObject() == false);
    Q_ASSERT(nextBinding() == 0);
    Q_ASSERT(*m_mePtr == 0);
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

    QObject *obj = object();
    Q_ASSERT(obj);

    int index = propertyIndex();

    QQmlData *data = QQmlData::get(obj, true);

    if (index & 0xFFFF0000) {
        // Value type

        int coreIndex = index & 0x0000FFFF;

        // Find the value type proxy (if there is one)
        QQmlValueTypeProxyBinding *proxy = 0;
        if (data->hasBindingBit(coreIndex)) {
            QQmlAbstractBinding *b = data->bindings;
            while (b && b->propertyIndex() != coreIndex)
                b = b->nextBinding();
            Q_ASSERT(b && b->bindingType() == QQmlAbstractBinding::ValueTypeProxy);
            proxy = static_cast<QQmlValueTypeProxyBinding *>(b);
        }

        if (!proxy) {
            proxy = new QQmlValueTypeProxyBinding(obj, coreIndex);

            Q_ASSERT(proxy->propertyIndex() == coreIndex);
            Q_ASSERT(proxy->object() == obj);

            proxy->addToObject();
        }

        setNextBinding(proxy->m_bindings);
        proxy->m_bindings = this;

    } else {
        setNextBinding(data->bindings);
        data->bindings = this;

        data->setBindingBit(obj, index);
    }

    setAddedToObject(true);
}

/*!
Remove the binding from the object.
*/
void QQmlAbstractBinding::removeFromObject()
{
    if (isAddedToObject()) {
        QObject *obj = object();
        int index = propertyIndex();

        QQmlData *data = QQmlData::get(obj, false);
        Q_ASSERT(data);

        if (index & 0xFFFF0000) {

            // Find the value type binding
            QQmlAbstractBinding *vtbinding = data->bindings;
            while (vtbinding->propertyIndex() != (index & 0x0000FFFF)) {
                vtbinding = vtbinding->nextBinding();
                Q_ASSERT(vtbinding);
            }
            Q_ASSERT(vtbinding->bindingType() == QQmlAbstractBinding::ValueTypeProxy);

            QQmlValueTypeProxyBinding *vtproxybinding =
                static_cast<QQmlValueTypeProxyBinding *>(vtbinding);

            QQmlAbstractBinding *binding = vtproxybinding->m_bindings;
            if (binding == this) {
                vtproxybinding->m_bindings = nextBinding();
            } else {
               while (binding->nextBinding() != this) {
                  binding = binding->nextBinding();
                  Q_ASSERT(binding);
               }
               binding->setNextBinding(nextBinding());
            }

            // Value type - we don't remove the proxy from the object.  It will sit their happily
            // doing nothing until it is removed by a write, a binding change or it is reused
            // to hold more sub-bindings.

        } else {

            if (data->bindings == this) {
                data->bindings = nextBinding();
            } else {
                QQmlAbstractBinding *binding = data->bindings;
                while (binding->nextBinding() != this) {
                    binding = binding->nextBinding();
                    Q_ASSERT(binding);
                }
                binding->setNextBinding(nextBinding());
            }

            data->clearBindingBit(index);
        }

        setNextBinding(0);
        setAddedToObject(false);
    }
}

void QQmlAbstractBinding::printBindingLoopError(QQmlProperty &prop)
{
    qmlInfo(prop.object()) << QString(QLatin1String("Binding loop detected for property \"%1\"")).arg(prop.name());
}


static void bindingDummyDeleter(QQmlAbstractBinding *)
{
}

QQmlAbstractBinding::Pointer QQmlAbstractBinding::weakPointer()
{
    if (m_mePtr.value().isNull())
        m_mePtr.value() = QSharedPointer<QQmlAbstractBinding>(this, bindingDummyDeleter);

    return m_mePtr.value().toWeakRef();
}

void QQmlAbstractBinding::clear()
{
    if (!m_mePtr.isNull()) {
        **m_mePtr = 0;
        m_mePtr = 0;
    }
}

void QQmlAbstractBinding::default_retargetBinding(QQmlAbstractBinding *, QObject *, int)
{
    qFatal("QQmlAbstractBinding::retargetBinding() called on illegal binding.");
}

QString QQmlAbstractBinding::default_expression(const QQmlAbstractBinding *)
{
    return QLatin1String("<Unknown>");
}

QT_END_NAMESPACE
