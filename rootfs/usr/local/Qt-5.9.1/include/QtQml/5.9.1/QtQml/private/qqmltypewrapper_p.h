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

#ifndef QV8TYPEWRAPPER_P_H
#define QV8TYPEWRAPPER_P_H

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

#include <QtCore/qglobal.h>
#include <QtCore/qpointer.h>

#include <private/qv4value_p.h>
#include <private/qv4object_p.h>

QT_BEGIN_NAMESPACE

class QQmlTypeNameCache;

namespace QV4 {

namespace Heap {

struct QmlTypeWrapper : Object {
    enum TypeNameMode {
        IncludeEnums,
        ExcludeEnums
    };

    void init();
    void destroy();
    TypeNameMode mode;
    QQmlQPointer<QObject> object;

    QQmlType *type;
    QQmlTypeNameCache *typeNamespace;
    const void *importNamespace;
};

}

struct Q_QML_EXPORT QmlTypeWrapper : Object
{
    V4_OBJECT2(QmlTypeWrapper, Object)
    V4_NEEDS_DESTROY

    bool isSingleton() const;
    QObject *singletonObject() const;

    QVariant toVariant() const;

    static ReturnedValue create(ExecutionEngine *, QObject *, QQmlType *,
                                Heap::QmlTypeWrapper::TypeNameMode = Heap::QmlTypeWrapper::IncludeEnums);
    static ReturnedValue create(ExecutionEngine *, QObject *, QQmlTypeNameCache *, const void *,
                                Heap::QmlTypeWrapper::TypeNameMode = Heap::QmlTypeWrapper::IncludeEnums);


    static ReturnedValue get(const Managed *m, String *name, bool *hasProperty);
    static void put(Managed *m, String *name, const Value &value);
    static PropertyAttributes query(const Managed *, String *name);
    static bool isEqualTo(Managed *that, Managed *o);

};

}

QT_END_NAMESPACE

#endif // QV8TYPEWRAPPER_P_H

