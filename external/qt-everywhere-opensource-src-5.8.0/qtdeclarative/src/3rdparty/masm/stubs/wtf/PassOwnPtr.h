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
#ifndef PASSOWNPTR_H
#define PASSOWNPTR_H

#include <qscopedpointer.h>

template <typename T> class PassOwnPtr;
template <typename PtrType> PassOwnPtr<PtrType> adoptPtr(PtrType*);

template <typename T>
struct OwnPtr : public QScopedPointer<T>
{
    OwnPtr() {}
    OwnPtr(const PassOwnPtr<T> &ptr)
        : QScopedPointer<T>(ptr.leakRef())
    {}

    OwnPtr(const OwnPtr<T>& other)
        : QScopedPointer<T>(const_cast<OwnPtr<T> &>(other).take())
    {}

    OwnPtr& operator=(const OwnPtr<T>& other)
    {
        this->reset(const_cast<OwnPtr<T> &>(other).take());
        return *this;
    }

    T* get() const { return this->data(); }

    PassOwnPtr<T> release()
    {
        return adoptPtr(this->take());
    }
};

template <typename T>
class PassOwnPtr {
public:
    PassOwnPtr() {}

    PassOwnPtr(T* ptr)
        : m_ptr(ptr)
    {
    }

    PassOwnPtr(const PassOwnPtr<T>& other)
        : m_ptr(other.leakRef())
    {
    }

    PassOwnPtr(const OwnPtr<T>& other)
        : m_ptr(other.take())
    {
    }

    ~PassOwnPtr()
    {
    }

    T* operator->() const { return m_ptr.data(); }

    T* leakRef() const { return m_ptr.take(); }

private:
    template <typename PtrType> friend PassOwnPtr<PtrType> adoptPtr(PtrType*);

    PassOwnPtr<T>& operator=(const PassOwnPtr<T>& t);

    mutable QScopedPointer<T> m_ptr;
};

template <typename T>
PassOwnPtr<T> adoptPtr(T* ptr)
{
    PassOwnPtr<T> result;
    result.m_ptr.reset(ptr);
    return result;
}


#endif // PASSOWNPTR_H
