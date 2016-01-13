// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Nullable_h
#define Nullable_h

#include "wtf/Assertions.h"

namespace WebCore {

template <typename T>
class Nullable {
public:
    Nullable()
        : m_value()
        , m_isNull(true) { }

    Nullable(const T& value)
        : m_value(value)
        , m_isNull(false) { }

    Nullable(const Nullable& other)
        : m_value(other.m_value)
        , m_isNull(other.m_isNull) { }

    Nullable& operator=(const Nullable& other)
    {
        m_value = other.m_value;
        m_isNull = other.m_isNull;
        return *this;
    }

    T get() const { ASSERT(!m_isNull); return m_value; }
    bool isNull() const { return m_isNull; }

    operator bool() const { return !m_isNull && m_value; }

    bool operator==(const Nullable& other) const
    {
        return (m_isNull && other.m_isNull) || (!m_isNull && !other.m_isNull && m_value == other.m_value);
    }

private:
    T m_value;
    bool m_isNull;
};

} // namespace WebCore

#endif // Nullable_h
