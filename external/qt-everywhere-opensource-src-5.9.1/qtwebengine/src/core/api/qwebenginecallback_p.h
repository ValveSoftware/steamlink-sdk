/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef QWEBENGINECALLBACK_P_H
#define QWEBENGINECALLBACK_P_H

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

#include "qtwebenginecoreglobal_p.h"
#include "qwebenginecallback.h"

#include <QByteArray>
#include <QHash>
#include <QSharedData>
#include <QString>
#include <QVariant>
#include <type_traits>

// keep in sync with Q_DECLARE_SHARED... in qwebenginecallback.h
#define FOR_EACH_TYPE(F) \
    F(bool) \
    F(int) \
    F(const QString &) \
    F(const QByteArray &) \
    F(const QVariant &)

namespace QtWebEngineCore {

class CallbackDirectory {
    template<typename T>
    void invokeInternal(quint64 callbackId, T result);
    template<typename T>
    void invokeEmptyInternal(QtWebEnginePrivate::QWebEngineCallbackPrivateBase<T> *callback);

public:
    ~CallbackDirectory()
    {
        // "Cancel" pending callbacks by calling them with an invalid value.
        // This guarantees that each callback is called exactly once.
        for (CallbackSharedDataPointerBase * const sharedPtrBase: m_callbackMap) {
            Q_ASSERT(sharedPtrBase);
            sharedPtrBase->invokeEmpty();
            delete sharedPtrBase;
        }
    }

    enum ReservedCallbackIds {
        NoCallbackId = 0,
        DeleteCookieCallbackId,
        DeleteSessionCookiesCallbackId,
        DeleteAllCookiesCallbackId,
        GetAllCookiesCallbackId,

        // Place reserved id's before this.
        ReservedCallbackIdsEnd
    };

    template<typename T>
    void registerCallback(quint64 callbackId, const QWebEngineCallback<T> &callback);

    template<typename T>
    void invokeEmpty(const QWebEngineCallback<T> &callback);

#define DEFINE_INVOKE_FOR_TYPE(Type) \
    void invoke(quint64 callbackId, Type result) { \
        invokeInternal<Type>(callbackId, std::forward<Type>(result)); \
    }
    FOR_EACH_TYPE(DEFINE_INVOKE_FOR_TYPE)
#undef DEFINE_INVOKE_FOR_TYPE

    template <typename A>
    void invokeDirectly(const QWebEngineCallback<A> &callback, const A &argument)
    {
        return callback.d.data()->operator()(std::forward<const A&>(argument));
    }

private:
    struct CallbackSharedDataPointerBase {
        virtual ~CallbackSharedDataPointerBase() { }
        virtual void invokeEmpty() = 0;
        virtual void doRef() = 0;
        virtual void doDeref() = 0;
        virtual operator bool () const = 0;
    };

    template <typename T>
    struct CallbackSharedDataPointer : public CallbackSharedDataPointerBase {
        CallbackDirectory* parent;
        QtWebEnginePrivate::QWebEngineCallbackPrivateBase<T> *callback;

        ~CallbackSharedDataPointer() { doDeref(); }
        CallbackSharedDataPointer() : parent(0), callback(0) { }
        CallbackSharedDataPointer(const CallbackSharedDataPointer<T> &other)
            : parent(other.parent), callback(other.callback) { doRef(); }
        CallbackSharedDataPointer(CallbackDirectory *p, QtWebEnginePrivate::QWebEngineCallbackPrivateBase<T> *c)
            : parent(p), callback(c) { Q_ASSERT(callback); doRef(); }

        void invokeEmpty() override;
        operator bool () const override { return callback; }

    private:
        void doRef() override;
        void doDeref() override;
    };

    QHash<quint64, CallbackSharedDataPointerBase*> m_callbackMap;
};

template<typename T>
inline
void CallbackDirectory::registerCallback(quint64 callbackId, const QWebEngineCallback<T> &callback)
{
    if (!callback.d)
        return;
    m_callbackMap.insert(callbackId, new CallbackSharedDataPointer<T>(this, callback.d.data()));
}

template<typename T>
inline
void CallbackDirectory::invokeInternal(quint64 callbackId, T result)
{
    CallbackSharedDataPointerBase * const sharedPtrBase = m_callbackMap.take(callbackId);
    if (!sharedPtrBase)
        return;

    auto ptr = static_cast<CallbackSharedDataPointer<T> *>(sharedPtrBase);
    Q_ASSERT(ptr);
    (*ptr->callback)(std::forward<T>(result));
    delete ptr;
}

template<typename T>
inline
void CallbackDirectory::invokeEmptyInternal(QtWebEnginePrivate::QWebEngineCallbackPrivateBase<T> *callback)
{
    Q_ASSERT(callback);
    using NoRefT = typename std::remove_reference<T>::type;
    using NoConstNoRefT = typename std::remove_const<NoRefT>::type;
    NoConstNoRefT t;
    (*callback)(t);
}

template<>
inline
void CallbackDirectory::invokeEmptyInternal(QtWebEnginePrivate::QWebEngineCallbackPrivateBase<bool> *callback)
{
    Q_ASSERT(callback);
    (*callback)(false);
}

template<>
inline
void CallbackDirectory::invokeEmptyInternal(QtWebEnginePrivate::QWebEngineCallbackPrivateBase<int> *callback)
{
    Q_ASSERT(callback);
    (*callback)(0);
}

template<typename T>
inline
void CallbackDirectory::invokeEmpty(const QWebEngineCallback<T> &callback)
{
    if (!callback.d)
        return;

    invokeEmptyInternal(callback.d.data());
}

template <typename T>
inline
void CallbackDirectory::CallbackSharedDataPointer<T>::doRef()
{
    if (!callback)
        return;

    callback->ref.ref();
}

template <typename T>
inline
void CallbackDirectory::CallbackSharedDataPointer<T>::doDeref()
{
    if (!callback)
        return;
    if (!callback->ref.deref())
        delete callback;
}

template <typename T>
inline
void CallbackDirectory::CallbackSharedDataPointer<T>::invokeEmpty()
{
    if (!callback)
        return;

    Q_ASSERT(parent);
    parent->invokeEmptyInternal(callback);
}

#if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
#define CHECK_RELOCATABLE(x) \
  Q_STATIC_ASSERT((QTypeInfoQuery<QWebEngineCallback< x > >::isRelocatable));
FOR_EACH_TYPE(CHECK_RELOCATABLE)
#undef CHECK_RELOCATABLE
#endif

} // namespace QtWebEngineCore

#endif // QWEBENGINECALLBACK_P_H
