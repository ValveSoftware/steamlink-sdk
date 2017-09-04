/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#ifndef QREMOTEOBJECTSOURCE_H
#define QREMOTEOBJECTSOURCE_H

#include <QtCore/QScopedPointer>
#include <QtRemoteObjects/qtremoteobjectglobal.h>
#include <QtCore/QMetaMethod>

QT_BEGIN_NAMESPACE

//Based on compile time checks for static connect() from qobjectdefs_impl.h
template <class ObjectType, typename Func1, typename Func2>
static inline int qtro_prop_index(Func1, Func2, const char *propName)
{
    typedef QtPrivate::FunctionPointer<Func1> Type1;
    typedef QtPrivate::FunctionPointer<Func2> Type2;

    //compilation error if the arguments do not match.
    Q_STATIC_ASSERT_X(int(Type1::ArgumentCount) >= int(Type2::ArgumentCount),
                      "Argument counts are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::CheckCompatibleArguments<typename Type1::Arguments, typename Type2::Arguments>::value),
                      "Arguments are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible<typename Type1::ReturnType, typename Type2::ReturnType>::value),
                      "Return types are not compatible.");
    return ObjectType::staticMetaObject.indexOfProperty(propName);
}

template <class ObjectType, typename Func1, typename Func2>
static inline int qtro_signal_index(Func1 func, Func2, int *count, int const **types)
{
    typedef QtPrivate::FunctionPointer<Func1> Type1;
    typedef QtPrivate::FunctionPointer<Func2> Type2;

    //compilation error if the arguments do not match.
    Q_STATIC_ASSERT_X(int(Type1::ArgumentCount) >= int(Type2::ArgumentCount),
                      "Argument counts are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::CheckCompatibleArguments<typename Type1::Arguments, typename Type2::Arguments>::value),
                      "Arguments are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible<typename Type1::ReturnType, typename Type2::ReturnType>::value),
                      "Return types are not compatible.");
    const QMetaMethod sig = QMetaMethod::fromSignal(func);
    *count = Type2::ArgumentCount;
    *types = QtPrivate::ConnectionTypes<typename Type2::Arguments>::types();
    return sig.methodIndex();
}

template <class ObjectType, typename Func1, typename Func2>
static inline void qtro_method_test(Func1, Func2)
{
    typedef QtPrivate::FunctionPointer<Func1> Type1;
    typedef QtPrivate::FunctionPointer<Func2> Type2;

    //compilation error if the arguments do not match.
    Q_STATIC_ASSERT_X(int(Type1::ArgumentCount) >= int(Type2::ArgumentCount),
                      "Argument counts are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::CheckCompatibleArguments<typename Type1::Arguments, typename Type2::Arguments>::value),
                      "Arguments are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible<typename Type1::ReturnType, typename Type2::ReturnType>::value),
                      "Return types are not compatible.");
}

template <class ObjectType, typename Func1, typename Func2>
static inline int qtro_method_index(Func1, Func2, const char *methodName, int *count, int const **types)
{
    typedef QtPrivate::FunctionPointer<Func1> Type1;
    typedef QtPrivate::FunctionPointer<Func2> Type2;

    //compilation error if the arguments do not match.
    Q_STATIC_ASSERT_X(int(Type1::ArgumentCount) >= int(Type2::ArgumentCount),
                      "Argument counts are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::CheckCompatibleArguments<typename Type1::Arguments, typename Type2::Arguments>::value),
                      "Arguments are not compatible.");
    Q_STATIC_ASSERT_X((QtPrivate::AreArgumentsCompatible<typename Type1::ReturnType, typename Type2::ReturnType>::value),
                      "Return types are not compatible.");
    *count = Type2::ArgumentCount;
    *types = QtPrivate::ConnectionTypes<typename Type2::Arguments>::types();
    return ObjectType::staticMetaObject.indexOfMethod(methodName);
}

QByteArray qtro_classinfo_signature(const QMetaObject *metaObject);

class QRemoteObjectHostBase;
class SourceApiMap
{
protected:
    SourceApiMap() {}
public:
    virtual ~SourceApiMap() {}
    virtual QString name() const = 0;
    virtual QString typeName() const = 0;
    virtual int propertyCount() const = 0;
    virtual int signalCount() const = 0;
    virtual int methodCount() const = 0;
    virtual int modelCount() const { return 0; }
    virtual int sourcePropertyIndex(int index) const = 0;
    virtual int sourceSignalIndex(int index) const = 0;
    virtual int sourceMethodIndex(int index) const = 0;
    virtual int signalParameterCount(int index) const = 0;
    virtual int signalParameterType(int sigIndex, int paramIndex) const = 0;
    virtual const QByteArray signalSignature(int index) const = 0;
    virtual int methodParameterCount(int index) const = 0;
    virtual int methodParameterType(int methodIndex, int paramIndex) const = 0;
    virtual const QByteArray methodSignature(int index) const = 0;
    virtual QMetaMethod::MethodType methodType(int index) const = 0;
    virtual const QByteArray typeName(int index) const = 0;
    virtual int propertyIndexFromSignal(int index) const = 0;
    virtual int propertyRawIndexFromSignal(int index) const = 0;
    virtual QByteArray objectSignature() const = 0;
    virtual bool isDynamic() const { return false; }
    virtual bool isAdapterSignal(int) const { return false; }
    virtual bool isAdapterMethod(int) const { return false; }
    virtual bool isAdapterProperty(int) const { return false; }
    virtual void modelSetup(QRemoteObjectHostBase *) const { }
};

QT_END_NAMESPACE

#endif
