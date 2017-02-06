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

#include "qqmlproxymetaobject_p.h"
#include "qqmlproperty_p.h"

QT_BEGIN_NAMESPACE

QQmlProxyMetaObject::QQmlProxyMetaObject(QObject *obj, QList<ProxyData> *mList)
: metaObjects(mList), proxies(0), parent(0), object(obj)
{
    *static_cast<QMetaObject *>(this) = *metaObjects->constFirst().metaObject;

    QObjectPrivate *op = QObjectPrivate::get(obj);
    if (op->metaObject)
        parent = static_cast<QAbstractDynamicMetaObject*>(op->metaObject);

    op->metaObject = this;
}

QQmlProxyMetaObject::~QQmlProxyMetaObject()
{
    if (parent)
        delete parent;
    parent = 0;

    if (proxies)
        delete [] proxies;
    proxies = 0;
}

int QQmlProxyMetaObject::metaCall(QObject *o, QMetaObject::Call c, int id, void **a)
{
    Q_ASSERT(object == o);

    if ((c == QMetaObject::ReadProperty ||
        c == QMetaObject::WriteProperty) &&
            id >= metaObjects->constLast().propertyOffset) {

        for (int ii = 0; ii < metaObjects->count(); ++ii) {
            const ProxyData &data = metaObjects->at(ii);
            if (id >= data.propertyOffset) {
                if (!proxies) {
                    proxies = new QObject*[metaObjects->count()];
                    ::memset(proxies, 0,
                             sizeof(QObject *) * metaObjects->count());
                }

                if (!proxies[ii]) {
                    QObject *proxy = data.createFunc(object);
                    const QMetaObject *metaObject = proxy->metaObject();
                    proxies[ii] = proxy;

                    int localOffset = data.metaObject->methodOffset();
                    int methodOffset = metaObject->methodOffset();
                    int methods = metaObject->methodCount() - methodOffset;

                    // ### - Can this be done more optimally?
                    for (int jj = 0; jj < methods; ++jj) {
                        QMetaMethod method =
                            metaObject->method(jj + methodOffset);
                        if (method.methodType() == QMetaMethod::Signal)
                            QQmlPropertyPrivate::connect(proxy, methodOffset + jj, object, localOffset + jj);
                    }
                }

                int proxyOffset = proxies[ii]->metaObject()->propertyOffset();
                int proxyId = id - data.propertyOffset + proxyOffset;

                return proxies[ii]->qt_metacall(c, proxyId, a);
            }
        }
    } else if (c == QMetaObject::InvokeMetaMethod &&
               id >= metaObjects->constLast().methodOffset) {
        QMetaMethod m = object->metaObject()->method(id);
        if (m.methodType() == QMetaMethod::Signal) {
            QMetaObject::activate(object, id, a);
            return -1;
        }
    }

    if (parent)
        return parent->metaCall(o, c, id, a);
    else
        return object->qt_metacall(c, id, a);
}

QT_END_NAMESPACE
