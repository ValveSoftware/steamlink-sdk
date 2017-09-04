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
#ifndef VECTOR_H
#define VECTOR_H

#include <vector>
#include <wtf/Assertions.h>
#include <wtf/NotFound.h>
#include <qalgorithms.h>

enum WTF_UnusedOverflowMode {
    UnsafeVectorOverflow
};

namespace WTF {

template <typename T, int capacity = 1, int overflowMode = UnsafeVectorOverflow>
class Vector : public std::vector<T> {
public:
    Vector() {}
    Vector(int initialSize) : std::vector<T>(initialSize) {}

    inline void append(const T& value)
    { this->push_back(value); }

    template <typename OtherType>
    inline void append(const OtherType& other)
    { this->push_back(T(other)); }

    inline void append(const Vector<T>& vector)
    {
        this->insert(this->end(), vector.begin(), vector.end());
    }

    inline void append(const T* ptr, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            this->push_back(T(ptr[i]));
    }

    inline void append(typename std::vector<T>::const_iterator it, size_t count)
    {
        for (size_t i = 0; i < count; ++i, ++it)
            this->push_back(*it);
    }

    using std::vector<T>::insert;

    inline void reserveInitialCapacity(size_t size) { this->reserve(size); }

    inline void insert(size_t position, T value)
    { this->insert(this->begin() + position, value); }

    inline void grow(size_t size)
    { this->resize(size); }

    inline void shrink(size_t size)
    { this->erase(this->begin() + size, this->end()); }

    inline void shrinkToFit()
    { this->shrink(this->size()); }

    inline void remove(size_t position)
    { this->erase(this->begin() + position); }

    inline bool isEmpty() const { return this->empty(); }

    inline T &last() { return *(this->begin() + this->size() - 1); }
};

template <typename T, int capacity>
void deleteAllValues(const Vector<T, capacity> &vector)
{
    qDeleteAll(vector);
}

}

using WTF::Vector;
using WTF::deleteAllValues;

#endif // VECTOR_H
